# Phase 2 — Position et FEN

## Architecture et fichiers

`Position` possède le plateau et son état courant. Elle ne possède ni flux UCI,
ni historique de partie, ni ressource Qt. Les couches futures pourront consulter
ses bitboards sans connaître la syntaxe FEN. Le code complet est dans :

- `src/board/position.hpp` : interface, CastlingRights, GameState et layout.
- `src/board/position.cpp` : initialisation, validation, parsing et sérialisation.
- `tests/position_tests.cpp` : cinq nouvelles suites.
- `tests/test_support.hpp` : harnais partagé avec les tests de phase 1.
- `src/main.cpp` : diagnostic mémoire et commande `--fen`.
- `CMakeLists.txt` : `chess_core` devient une bibliothèque statique C++20.

Les en-têtes de phase 1 restent utilisables tels quels. Le moteur ne génère et
ne joue encore aucun coup. Les attaques seront construites en phase 3. Le hash
Zobrist sera ajouté en phase 12, avec ses clés et ses tests ; aucun champ factice
à zéro n'est présenté comme un hash valide.

## Une représentation hybride

La position contient six bitboards par type (pions, cavaliers, fous, tours, dames,
rois), réunissant les deux couleurs, et deux bitboards par couleur. Par exemple :

```cpp
const chess::Position position;
const auto whiteKnights = position.pieces(chess::Color::White, chess::PieceType::Knight);
const auto occupied = position.occupancy();
const auto piece = position.pieceAt(chess::Square::E1);
```

L'intersection d'un type et d'une couleur donne un ensemble spécifique. Une
union des deux couleurs donne l'occupation. Stocker directement douze bitboards
couleur/type économiserait l'intersection mais prendrait 96 octets au lieu de 64
pour ces ensembles (davantage si les occupations par couleur sont aussi conservées).
Ce choix pourra être réévalué à partir des profils de génération/recherche.

Le tableau `board_` de 64 Piece ajoute une lecture directe de case. Avec seulement
les bitboards, identifier une pièce demande de tester des ensembles ; ce tableau
rend `pieceAt` simple et constant. Le compromis est une redondance : chaque
placement doit actualiser le tableau et les deux bitboards concernés.

`putPiece` est privé et exige une pièce non vide sur une case vide. Aucun setter
public ne permet de modifier un seul des trois ensembles. Les opérations privées
nécessaires à make/unmake seront ajoutées en phase 5. Le constructeur public
produit toujours la position initiale ; le constructeur privé EmptyTag n'existe
que pour bâtir une position temporaire dans le parseur.

## Invariants

`isConsistent()` reconstruit les bitboards à partir du tableau et les compare.
Cette égalité vérifie simultanément l'absence de recouvrement entre couleurs,
l'absence de recouvrement entre types et la cohérence de l'occupation.
Elle vérifie également :

- exactement un roi de chaque couleur ;
- au plus huit pions et seize pièces par couleur ;
- aucun pion sur les rangées 1 et 8 ;
- un trait valide, des droits de roque sur quatre bits et un numéro de coup positif ;
- pour chaque droit de roque déclaré, le roi et la tour de cette couleur sur leurs cases initiales ;
- une éventuelle cible en passant vide, au bon rang, avec le pion adverse au bon endroit,
  sa case d'origine vide et un compteur de demi-coups nul.

Ces vérifications assurent une cohérence structurelle, pas la légalité complète.
Elles ne détectent pas encore les rois adjacents, un roi laissé en échec, ni une
position historiquement impossible à atteindre. Elles ne déduisent jamais un
droit de roque de la présence du roi et de la tour : ces pièces ont pu bouger.
Un droit de roque peut exister même si le chemin est occupé ou attaqué.

Il n'y a pas d'historique implicite. Deux positions ayant le même plateau peuvent
avoir des droits, compteurs ou traits différents ; `operator==` compare tous les
membres, sans comparer le padding brut avec memcmp. La répétition future aura
une définition dédiée, qui ne sera pas cette égalité complète des positions.

## GameState et compteurs

GameState regroupe le trait, les quatre droits de roque, la case en passant et
les deux compteurs FEN. `state()` renvoie une petite copie : modifier cette copie
ne modifie pas Position et ne crée aucune référence susceptible de devenir pendante.

Les compteurs sont des uint32_t : pas de troncature silencieuse au-delà de 255
ou 65535. HalfmoveClock commence à zéro ; FullmoveNumber commence à un et sera
incrémenté après les coups noirs. Les valeurs supérieures à UINT32_MAX sont
rejetées. La politique d'incrément à cette limite devra être définie avec makeMove
(par exemple saturation), plutôt que laisser apparaître un retour à zéro.

CastlingRights est un enum class sur un octet, traité comme masque de quatre bits.
`hasRight` vérifie tous les bits demandés et renvoie false pour None : demander
« aucun droit » ne signifie pas que la position n'a aucun droit. Pour ce dernier
cas, comparer explicitement à CastlingRights::None.

## FEN : frontière vérifiée

Exemple de position initiale :

```text
rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
```

Les six champs sont le placement (rangée 8 vers rangée 1), le trait, les droits
de roque, la cible en passant, le compteur de demi-coups et le numéro du coup.
Les majuscules désignent les blancs. Un chiffre représente une suite de cases vides.

Le contrat de notre lecteur est explicite :

- exactement six champs, avec séparateurs ASCII blancs ; les espaces externes sont permis ;
- exactement huit rangées de huit cases, lettres de pièces reconnues et chiffres 1..8 ;
- les chiffres de cases vides adjacents sont rejetés : écrire `8`, pas `44` ;
- trait `w` ou `b`, droits sans doublon parmi KQkq ou `-` ;
- l'ordre des droits en entrée est tolérant, mais la sortie utilise KQkq ;
- compteurs décimaux non signés, sans suffixe, avec contrôle de débordement ;
- les zéros initiaux des compteurs sont acceptés puis normalisés ;
- les invariants structurels précédents doivent être satisfaits.

Ce lecteur vise les positions orthodoxes du moteur, pas un éditeur de diagrammes
arbitraires sans roi ni les conventions de roque Chess960. Les rejets de droits
incohérents et d'en passant impossible sont des choix de validation du moteur,
au-delà du découpage syntaxique des champs.

Après e2e4, la cible e3 est conservée même si aucun pion noir ne peut la capturer :

```text
rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1
```

Le FEN enregistre cette cible ; l'existence d'une capture légale sera une autre
question pour le générateur. Le parseur n'invente pas une cible lorsqu'il reçoit `-`.

```cpp
std::string error;
auto parsed = chess::Position::fromFen(chess::Position::StartFen, &error);
if (parsed) {
    const std::string canonicalFen = parsed->toFen();
}
```

Une erreur de contenu retourne nullopt et, si demandé, un message. Le parseur
n'expose jamais une position à moitié chargée. Pour remplacer une position
existante sans la perdre en cas d'erreur, n'affecter que si l'optional est engagé.
La construction temporaire copie éventuellement une Position une seule fois à
la frontière ; cela ne préjuge pas de make/unmake, qui évitera les copies par nœud.

## Durées de vie, exceptions et complexité

Le découpage utilise six string_view locales pointant vers l'entrée. Elles ne
sont pas conservées dans Position. La chaîne source peut être détruite dès le
retour. Même si `error` désigne la chaîne source, le message n'est écrit qu'au
retour d'échec, et le diagnostic n'est effacé qu'après le dernier accès à l'entrée.
Un test couvre ce cas d'aliasing.

`std::from_chars` lit les compteurs sans allocation, sans dépendance à la locale
et sans exception pour les erreurs numériques. Le parseur lui-même n'est pas
noexcept : écrire le diagnostic peut allouer. `toFen` construit une std::string
propriétaire et peut également allouer. Les accès à Position et le validateur
sont noexcept et ne font pas d'allocation dynamique.

Le parsing coûte O(n) pour une entrée de n caractères ; les espaces et compteurs
peuvent être longs, même si le plateau a toujours 64 cases. La sérialisation et
la validation parcourent au plus 64 cases et un nombre fixe de métadonnées :
O(64), donc O(1) pour ce jeu. Les accès aux ensembles et à une case sont O(1).
La mémoire auxiliaire du parsing est fixe, hors éventuel message d'erreur ; la
chaîne de sortie a une longueur bornée avec nos compteurs de 32 bits.

## Layout mémoire et cache

Valeurs mesurées sous Linux x86-64, GCC 15 :

| Type | sizeof | alignof |
|---|---:|---:|
| GameState | 12 | 4 |
| Position | 144 | 8 |

Disposition correspondant à cette ABI :

```text
GameState
  0..3    halfmoveClock
  4..7    fullmoveNumber
  8       sideToMove
  9       castlingRights
 10       enPassantSquare
 11       padding

Position
  0..47   byType_  : 6 × 8 octets
 48..63   byColor_ : 2 × 8 octets
 64..127  board_   : 64 × 1 octet
128..139  state_   : 12 octets
140..143  padding final
```

Ces tailles ne sont pas des garanties sur toutes les ABI. Le programme les
imprime ; les static_assert garantissent les propriétés standard-layout et
trivially-copyable, sans imposer artificiellement une taille propre à Linux.
Le padding n'est ni une donnée échiquéenne ni un format de sérialisation.

Les huit bitboards sont regroupés sur 64 octets. S'ils commencent sur une frontière
de ligne de cache de 64 octets, ils tiennent dans une ligne ; l'alignement de 8 ne
le garantit pas. Le tableau par case est contigu sur les 64 octets suivants.
Une Position de 144 octets peut intersecter trois ou quatre lignes selon son
adresse. Forcer alignas(64) ferait typiquement passer sizeof à 192 octets ; ce
serait un compromis à mesurer, pas une amélioration automatique.

Nous ne conservons pas un troisième bitboard d'occupation totale : un OR suffit,
et évite un champ redondant supplémentaire à actualiser. En revanche, le tableau
par case est délibérément redondant car il sert un accès différent. On pourra
comparer ces choix avec des profils, une fois le générateur et la recherche corrects.

## Branches et abstractions

Le lecteur FEN contient plusieurs branches pour distinguer chiffres, pièces,
séparateurs et erreurs. Ce code de chargement n'est pas exécuté à chaque nœud de
recherche : simplifier sa validation pour gagner quelques branches serait prématuré.
Les comparaisons et messages explicites facilitent le diagnostic.

Les accesseurs bitboards font des accès directs et, si besoin, une intersection.
`pieceAt` est un accès indexé. Aucun appel virtuel, shared_ptr, mutex ou allocation
ne s'y trouve. Les assertions ajoutent des contrôles en Debug et disparaissent
avec NDEBUG. Les contrôles de FEN restent actifs en Release.

`isConsistent` est appelé explicitement par le parseur même en Release ; le
contrôle ne doit pas dépendre d'assert. Les appels supplémentaires dans les
assertions du constructeur et de toFen servent au diagnostic. La phase make/unmake
pourra placer des vérifications Debug après les mutations et mesurer leur coût.

## Tests et exercices

Les suites vérifient la position initiale contre des masques numériques connus,
les allers-retours sur sept positions (dont Kiwipete, une finale et des cibles
en passant pour chaque couleur), les seize combinaisons de roques, la normalisation,
les compteurs limites, les rejets syntaxiques et structurels, les invariants croisés,
les copies et la destruction de la chaîne source. Ce ne sont pas encore des tests
perft : leurs nombres de nœuds seront validés en phase 6.

Exercices facultatifs, sans bloquer les fonctions livrées :

1. Ajouter `emptySquares()` et vérifier que son intersection avec occupancy est
   nulle et que leur union couvre 64 cases. Expliquer pourquoi aucun champ n'est nécessaire.
2. Écrire un affichage ASCII séparé de Position, rangée 8 en haut. Pourquoi le
   placer dans l'adaptateur CLI plutôt que stocker du texte dans le cœur ?
3. Construire deux FEN au plateau identique mais aux droits de roque différents.
   Vérifier que Position les distingue. Pourquoi les droits ne se déduisent-ils pas du plateau ?
4. Proposer un layout sans tableau par case. Calculer son coût mémoire et décrire
   le travail supplémentaire de pieceAt, puis identifier un benchmark futur pertinent.

Avant la phase 3 : savoir expliquer l'intersection type/couleur, la redondance
contrôlée, l'absence d'historique dans FEN et la différence entre cohérence et légalité.
