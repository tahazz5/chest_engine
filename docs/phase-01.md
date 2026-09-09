# Phase 1 — valeurs et bitboards

## Architecture globale

La bibliothèque `chess_core` contient le domaine échiquéen. À cette étape, elle
est INTERFACE : ses petites fonctions sont définies dans les en-têtes, où le
compilateur peut les évaluer à la compilation ou les intégrer aux appelants.
Elle deviendra une bibliothèque compilée avec Position et les autres modules.

Dépendances prévues, de haut en bas :

```text
CLI / adaptateur UCI / éventuelle GUI Qt
                ↓
       recherche → évaluation
                ↓
       génération → Position
                ↓
      Move / bitboards / types
```

Le cœur ne dépendra ni de Qt ni des entrées/sorties UCI. L'adaptateur gérera les
commandes et le cycle de vie des recherches ; la recherche possédera ses piles
et sa table de transposition. Aucun de ces modules futurs n'est encore implémenté.

Fichiers actuels :

- `src/utils/types.hpp` : Color, PieceType, Piece, Square et conversions.
- `src/board/move.hpp` : MoveFlag et Move sur 16 bits.
- `src/board/bitboard.hpp` : masques et opérations ensemblistes.
- `src/main.cpp` : diagnostic du layout mémoire.
- `tests/core_tests.cpp` : harnais local et quatre suites CTest.
- `CMakeLists.txt` : bibliothèque, exécutable et tests portables.

## Types, contrats et cases

`enum class` empêche les conversions implicites entre couleur, pièce et case.
Le type sous-jacent uint8_t rend chaque valeur compacte. Cela ne garantit pas
qu'un cast explicite fournisse une valeur valide : `isValid` est donc disponible.
White et Black sont les seules couleurs. `opposite` exige une couleur valide.
Il n'y a pas de couleur « vide » ; `colorOf(Piece::None)` renvoie `nullopt`.

PieceType vaut None=0, Pawn=1, Knight=2, Bishop=3, Rook=4, Queen=5, King=6.
Piece encode vide=0, blancs=1..6, noirs=7..12. Ces codes denses permettront
une petite représentation par case si Position en a besoin. Un couple de deux
enums serait plus explicite mais occuperait deux octets par pièce au lieu d'un.
La division modulo 6 de `typeOf` est par une constante ; le compilateur peut
la remplacer par des opérations arithmétiques, sans division matérielle.
Nous n'affirmons aucun gain sans mesure. `colorOf` est sûr pour le vide au prix
d'un optional ; une version à précondition non-vide pourra être ajoutée si profilée.

Square suit `index = rank * 8 + file`, avec file/rank dans [0,7]. A1=0,
H1=7, A8=56, H8=63. None=64 est une sentinelle, jamais une case jouable.
Le rang croît vers le camp noir. Cette convention concerne les bits numériques,
pas l'ordre des octets en mémoire de la machine.

`makeSquare` et `parseSquare` sont des frontières vérifiées : les données invalides
renvoient None. `index`, `fileOf`, `rankOf`, `squareName` et `bit` exigent une
case valide. Les assertions signalent une violation en Debug ; en Release,
l'appelant reste responsable du contrat, notamment avant un décalage de bits.
Un futur parseur devra vérifier les entrées avant d'appeler les primitives rapides.

`squareName` retourne un tableau propriétaire de deux caractères, sans terminateur
nul. Utiliser une string_view avec sa longueur, jamais le traiter comme une chaîne C.
La string_view de `parseSquare` n'est pas conservée. Aucun pointeur vers une
variable locale n'est retourné, et aucune allocation n'est nécessaire.

## Encodage Move

```text
15             12 11              6 5               0
+----------------+-----------------+-----------------+
| flag (4 bits)  | arrivée (6 bits)| départ (6 bits) |
+----------------+-----------------+-----------------+
```

| Flag | Sens |
|---|---|
| 0 | calme |
| 1 | double poussée de pion |
| 2, 3 | petit, grand roque |
| 4 | capture |
| 5 | en passant, donc aussi capture |
| 6, 7 | réservés et rejetés |
| 8..11 | promotion cavalier, fou, tour, dame |
| 12..15 | mêmes promotions avec capture |

Le bit 14 indique capture ; le bit 15 indique promotion. Les deux bits de poids
faible du flag sélectionnent la pièce promue. Les helpers utilisent masques et
décalages, sans bitfields C++ dont la disposition dépendrait de l'implémentation.
Exemple : e2e4 avec double poussée = 12 | (28 << 6) | (1 << 12) = 0x170C.

Le constructeur exige deux cases valides distinctes et un flag défini. Il ne
vérifie pas la géométrie ni la présence d'une pièce : un coup encodé n'est pas
nécessairement pseudo-légal ou légal. Cette séparation évite de coupler Move à
Position. Les roques décrivent le déplacement du roi en échecs orthodoxes.
Chess960 demanderait une convention explicitement adaptée.

Le mot zéro est réservé à l'absence de coup. `from()` et `to()` renvoient None
pour ce mot, tous ses prédicats sont faux, et promotedPiece renvoie None.
Le null move de recherche sera une opération de Position, pas un coup légal.
`fromRaw` vérifie la structure d'un mot externe et retourne optional<Move> ; il
ne faut pas confondre cet optional avec une validation échiquéenne.

Une représentation 32 bits pourrait inclure la pièce mobile et la pièce capturée,
mais doublerait la taille d'un tableau de coups. Nous préférons retrouver ces
informations dans Position et UndoState plus tard. Le score de tri ne fait pas
partie de Move ; le stockage de coups scorés sera conçu lors du move ordering.

## Bitboards

Un uint64_t représente un ensemble de cases : le bit d'indice s indique la
présence de s. OR fait l'union, AND l'intersection, AND NOT la différence.
`set` et `clear` sont idempotents. Les constantes inline constexpr sont immuables.
L'alias Bitboard reste interchangeable avec uint64_t : il privilégie la notation
arithmétique. Un wrapper fort interdirait certains mélanges accidentels mais
nécessiterait davantage d'opérateurs et de conversions ; ce serait une alternative
raisonnable, pas intrinsèquement plus lente après optimisation.

Les déplacements nord/sud décalent de 8. Est/ouest masquent d'abord H/A pour
éviter que h1 ne devienne a2. Les diagonales composent ces opérations, sans
générer des coups : aucun obstacle ni règle de pièce n'est traité ici.

`std::popcount` compte les cases ; `std::countr_zero` localise la première.
Ces fonctions C++20 sont portables sans imposer BMI2 ou une instruction CPU
particulière. La sélection des instructions dépend du compilateur et de la cible.
`popLeastSquare` efface le bit bas par `b &= b - 1`. Sur zéro, le débordement
non signé est défini et le résultat reste zéro ; la fonction renvoie None.
Ne jamais écrire un décalage de 64 : sa validité n'est pas sauvée par uint64_t.

Toutes les opérations présentes sont O(1) sur un mot de taille fixe ; vider un
ensemble par popLeastSquare coûte O(k) pour k bits présents. Aucun rayon, table
d'attaques ou générateur de coups n'est construit dans cette phase.

## Layout, lifetime et matériel

| Type | sizeof garanti ici | Contenu |
|---|---:|---|
| Color, PieceType, Piece, Square | 1 chacun | entier sous-jacent uint8_t |
| Move | 2 | un uint16_t privé, aucun pointeur |
| Bitboard | 8 | uint64_t |

Des static_assert vérifient ces tailles ainsi que les propriétés trivially-copyable
et standard-layout de Move. L'alignement dépend de l'ABI ; l'exécutable imprime
sizeof et alignof sur la machine utilisée. On n'impose ni packing ni alignas(64)
sur chaque coup : cela gonflerait inutilement le stockage. L'ordre des octets
n'est pas un format de fichier portable ; une sérialisation future devra le définir.

Tous les objets sont des valeurs sans ressource externe. Les copies sont petites,
les références mutables de set/clear/popLeastSquare ne sont conservées nulle part.
`noexcept` décrit l'absence d'exceptions, pas une validation des arguments.
`constexpr` autorise l'évaluation à la compilation, mais ne l'impose pas au runtime.
`[[nodiscard]]` aide à repérer un résultat ignoré sans alourdir les objets.

Sur un CPU à lignes de cache de 64 octets, 32 Move tiennent dans une ligne si
le tableau est correctement positionné. Le caractère contigu aide la localité.
Cela ne suffit pas à prédire les performances d'une recherche : accès à Position,
TT et ordre des coups seront déterminants. Les primitives n'allouent pas sur le tas.

Les tests de flags et les décalages demandent peu d'opérations. Les cas sentinelles,
les frontières de parsing et certaines conversions introduisent des conditions ;
le compilateur peut choisir des branches ou des sélections sans branche. Une
branche rarement prise peut être peu coûteuse si bien prédite. Ni « constexpr »
ni une écriture sans if ne garantissent du code machine sans branche. Nous
mesurerons avant de remplacer une abstraction lisible par une variante spécialisée.

Position, MoveList, TTEntry et SearchStack n'existent pas encore : annoncer leurs
sizeof exacts serait artificiel. Lors de leur introduction, nous mesurerons taille,
alignement et padding. MoveList pourra utiliser un std::array de capacité justifiée
avec compteur ; TTEntry devra équilibrer clé complète, profondeur, score et coup ;
SearchStack sera préallouée par ply. Leur conception n'est pas figée ici.

## Tests et progression

Le petit harnais local nomme les suites, rapporte expression/fichier/ligne et
renvoie un statut d'échec à CTest. CHECK reste actif en Release, contrairement
à assert. Il pourra être remplacé par GoogleTest ou Catch2 si les besoins en
fixtures et diagnostics augmentent ; il n'en reproduit pas toutes les fonctionnalités.

Les suites couvrent les pièces/couleurs, les 64 conversions de cases, les entrées
invalides, les 65 536 mots possibles de Move, un encodage connu indépendant,
les huit décalages sur chaque case, les masques et les bitboards vides/pleins.
Les décalages sont comparés à un oracle en coordonnées, pas à une seconde
formule de décalage identique. Les perft viendront après génération et make/unmake ;
ils devront passer avant de démarrer l'évaluation et la recherche.

## Exercices facultatifs

1. Ajouter mirrorVertical(Square) : vérifier A1 → A8, E2 → E7 et l'involution
   sur les 64 cases. Décider et documenter le comportement sur None.
2. Ajouter moreThanOne(Bitboard) sans popcount ; tester vide, chaque singleton
   et des ensembles à plusieurs cases. Justifier le calcul non signé sur zéro.
3. Écrire une conversion Move vers texte long (`e2e4`, `a7a8q`) sans dépendance
   à Position. Définir le résultat pour l'absence de coup. Distinguer cette
   conversion d'un parseur capable de déterminer le flag à partir d'une position.
4. Dessiner un format 32 bits avec pièces mobile/capturée. Calculer la place
   de 256 coups dans les deux formats, puis expliquer pourquoi les octets seuls
   ne suffisent pas pour conclure sur le temps de recherche.

Avant la phase 2, vérifier conceptuellement : pourquoi None n'est pas une case,
pourquoi en passant est une capture, pourquoi le masque est nécessaire à l'est,
et pourquoi un Move structurellement valide peut rester illégal. La phase 2
attendra cette revue ; aucun générateur complet n'est ajouté maintenant.
