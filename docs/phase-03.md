# Phase 3 — tables et requêtes d’attaques

## Architecture et fichiers

Cette couche calcule les cases attaquées et les attaquants d’une case. Elle ne
construit aucun Move et ne modifie pas Position. Le générateur viendra en phase 4.

- `src/board/attacks.hpp` : tables constexpr, API des rayons, SlidingAttacks et requêtes.
- `src/board/attacks.cpp` : rayons, tables indexées, sélection PEXT et détection d’échec.
- `tests/attack_tests.cpp` : oracles indépendants et quatre nouvelles suites CTest.
- `benchmark/attack_bench.cpp` : microbenchmark reproductible, compilé sur demande.
- `CMakeLists.txt` : options CHESS_DISABLE_PEXT et CHESS_BUILD_BENCHMARKS.
- `src/main.cpp` : diagnostic mémoire et disponibilité de PEXT.

Les primitives ne dépendent que des types et bitboards. Les fonctions attackersTo,
isSquareAttacked et inCheck utilisent Position en lecture seule. Position ne dépend
pas de cette couche, ce qui évite une dépendance circulaire.

## Le contrat : attaque, destination et légalité

Une attaque glissante inclut le premier obstacle, puis s’arrête. Elle ne dépend
pas de la couleur de cet obstacle. Un pion attaque ses diagonales, jamais la case
devant lui. Un roi attaque les cases adjacentes ; le roque n’est pas une attaque.
La case de départ n’appartient jamais au résultat. Toutes les API exigent une case
valide ; les couleurs doivent également être valides.

Exemple : une tour en d4 avec un obstacle en d6 attaque d5 et d6, mais pas d7.
Pour construire ses destinations pseudo-légales, le futur générateur exclura
les cases occupées par son camp. La vérification de son propre roi viendra ensuite.

Une pièce clouée continue à attaquer géométriquement : il ne faut pas lui retirer
ses attaques au motif qu’elle ne peut pas jouer ce coup. C’est notamment nécessaire
pour les déplacements du roi, conformément à l’article 3.1.3 des
[règles FIDE](https://handbook.fide.com/chapter/e012023).

## Tables de pions, cavaliers et rois

Les attaques de ces pièces ne dépendent pas de l’occupation. On calcule une fois
les destinations de chaque case avec des coordonnées fichier/rang, en rejetant
celles qui sortent du plateau. Les tableaux sont inline constexpr : données
immuables avec durée de vie statique, sans initialisation dynamique requise.

```cpp
#include "board/attacks.hpp"

const auto knightTargets = chess::attacks::knight(chess::Square::B1);
const auto pawnTargets = chess::attacks::pawn(chess::Color::White, chess::Square::E2);
```

Le cavalier dispose de 64 entrées, le roi de 64, les pions de 2 × 64. Chaque entrée
est un bitboard de huit octets : 512 + 512 + 1024 = 2048 octets de résultats.
Le compilateur peut éliminer les petites descriptions de déplacements utilisées
uniquement pour leur construction à la compilation.

Chaque consultation est un accès indexé O(1). La table évite les tests de bords
à chaque requête. Des formules de décalage et masques seraient une alternative,
particulièrement pour attaquer avec un ensemble entier de pions ; elles pourront
être ajoutées si un usage concret le justifie.

## Référence simple : les rayons

rookRays parcourt quatre directions orthogonales, bishopRays quatre diagonales.
Dans chaque direction, on ajoute une case, puis on s’arrête si elle est occupée.
queenRays est l’union des deux résultats. Le bit d’occupation de la case de départ
est ignoré puisque le parcours commence sur la case suivante.

Sur un plateau de dimension variable, le coût est O(L), où L est la longueur des
rayons visités. Sur 8 × 8, il est borné : une tour visite au plus 14 destinations,
un fou 13. Aucun tableau d’occupation ni allocation n’est nécessaire.

Cette méthode est lisible et constitue une référence utile. Ses branches de sortie
dépendent des obstacles : leur prédictibilité varie avec les positions. Un plateau
dense raccourcit les rayons sans garantir un temps proportionnel au nombre de cases,
car la prédiction de branches compte également.

## Tables indexées : masque pertinent et PEXT

Pour une pièce glissante sur une case donnée, seules certaines cases d’occupation
changent le résultat. Les cases hors rayon et la case de départ sont sans effet.
La dernière case de chaque rayon est aussi sans effet : elle est atteinte ou non
à cause des obstacles précédents, et il n’existe aucune case derrière elle à couper.

Attention : on retire le dernier point de chaque rayon, pas toutes les cases du
bord. Pour une tour en a1, a2..a7 et b1..g1 restent pertinentes bien qu’elles soient
sur une bordure du plateau.

Le masque pertinent sélectionne entre 5 et 12 bits selon la pièce et la case.
Pour k bits, on stocke 2^k résultats. L’extraction compacte ces bits d’occupation
vers les bits bas d’un entier, en conservant leur ordre. Par exemple :

```text
occupation : 101000
masque     : 111000
index      :    101
```

extractBits implémente cette opération en C++ portable. PEXT réalise la même
extraction matériellement lorsqu’elle est disponible. La construction des tables
utilise toujours la version portable et les rayons, une seule fois par propriétaire.
Le sous-ensemble suivant se calcule par `(subset - mask) & mask`, en arithmétique
non signée ; tous les sous-ensembles sont visités, y compris l’ensemble vide.

Le total est de 102400 entrées pour les tours et 5248 pour les fous, soit
107648 bitboards ou 861184 octets de résultats. Une dame réutilise ces tables.

Le coût d’une consultation portable est O(k) pour extraire l’index, suivi d’un
accès mémoire. Avec PEXT, l’indexation utilise une instruction spécialisée, mais
cela ne garantit ni une latence identique sur tous les CPU, ni un accès mémoire
sans cache miss. La construction coûte O(somme des 2^k × (k + L)), hors allocation.

## Sélection et portabilité

```cpp
const chess::attacks::SlidingAttacks reference; // rayons par défaut
const chess::attacks::SlidingAttacks portable(chess::attacks::SlidingBackend::PortableTable);
const chess::attacks::SlidingAttacks automatic(chess::attacks::SlidingBackend::Auto);
const auto targets = automatic.rook(chess::Square::D4, 0);
```

| Demande | Méthode effectivement utilisée |
|---|---|
| Rays | Rayons, aucune grande table |
| PortableTable | Tables avec extraction logicielle |
| PextTable | PEXT si disponible, sinon tables portables |
| Auto | PEXT si disponible, sinon rayons |

backend() indique le choix effectif. Auto n’est pas un autotuner : il vérifie une
capacité, sans mesurer les performances. Le choix par défaut reste Rays.

L’implémentation matérielle est isolée dans une fonction ciblée BMI2, non intégrée
aux appelants. Le reste du moteur ne reçoit pas de flag global `-mbmi2` ou
`-march=native`. Le constructeur vérifie la disponibilité avant de sélectionner
ce chemin. La détection utilise les mécanismes décrits par la
[documentation GCC](https://gcc.gnu.org/onlinedocs/gcc-15.1.0/gcc/x86-Built-in-Functions.html).

Ce chemin ciblé est activé ici avec GCC/Clang sur x86-64 hors mode MSVC. Les autres
configurations, dont MSVC et ARM, utilisent les méthodes portables. Cela permet de
compiler le moteur sans exiger BMI2 partout ; Windows/MSVC n’a pas été exécuté
pendant cette phase. CHESS_DISABLE_PEXT force le repli même sur la machine de test.

Une autre solution serait les magic bitboards : multiplication d’une occupation
masquée et décalage pour obtenir un index, avec constantes validées et gestion
correcte des collisions. Cela évite l’exigence PEXT, mais ajoute une procédure de
construction et validation plus complexe. Nous avons choisi PEXT et son équivalent
logiciel pour garder une correspondance d’index directe et testable.

## Propriété, durée de vie et layout mémoire

SlidingAttacks possède ses Tables par unique_ptr<const Tables>. Le mode rayons
laisse ce pointeur vide. Les modes indexés font une allocation à la construction,
avant la recherche ; aucune requête n’alloue ni ne modifie les tables. Plusieurs
lecteurs peuvent partager une référence constante au même propriétaire.

La copie et le déplacement sont interdits : cela évite une copie accidentelle de
plus de 800 Kio et les états déplacés dont le pointeur ne correspondrait plus au
backend. Le propriétaire doit survivre à tous ses utilisateurs, notamment aux
futurs threads. Il ne faut pas construire une instance à chaque nœud de recherche.
Le destructeur est défini dans le .cpp où Tables est complet : unique_ptr peut
ainsi détruire son objet même si le type est caché dans l’en-tête.

La construction peut échouer par allocation et n’est pas noexcept. Les requêtes
le sont. RAII libère automatiquement les tables. Il n’y a ni état global mutable,
ni mutex, ni virtual dispatch dans le parcours des attaques.

Layout mesuré sur cette ABI Linux x86-64 :

| Objet | sizeof / contenu |
|---|---|
| SlidingAttacks | 16 octets, alignement 8 |
| Pointeur propriétaire | 8 octets sur cette ABI |
| Backend | 1 octet, suivi de padding dans le propriétaire |
| Tables::Entry | masque uint64_t + offset uint32_t + padding : 16 octets |
| Métadonnées | 128 Entry : 2048 octets |
| Résultats | 107648 × 8 : 861184 octets |
| Tables | 863232 octets, hors métadonnées internes de l’allocateur |

Le propriétaire occupe donc 16 octets même si son allocation distante est beaucoup
plus grande. tableBytes() expose la taille de cette allocation utile, zéro en mode
rayons. Ce n’est pas la consommation mémoire totale du processus. Les tailles et
le padding peuvent varier avec l’ABI ; aucun packing non portable n’est imposé.

Deux tableaux séparés de masques et offsets supprimeraient le padding de chaque
Entry et économiseraient environ 512 octets sur cette ABI, au prix d’un autre
agencement d’accès. Cette économie est modeste devant la table de résultats.

## Cache et branchement

Les petites tables des pièces non glissantes totalisent 2 Kio et peuvent bénéficier
d’une bonne localité. Les grandes tables glissantes occupent environ 843 Kio,
bien davantage qu’une petite zone de données chaude. Un index mène à un résultat
contigu de huit octets, mais les requêtes successives peuvent viser des lignes
très différentes ; elles entreront aussi en concurrence avec Position et la TT.

Les consultations font un test du backend qui reste stable pour un propriétaire.
PEXT appelle une fonction ciblée non inline pour maintenir l’isolation des
instructions. L’extraction portable contient une boucle et un test de chaque bit.
Le compilateur peut traduire certains tests en sélections sans branche ; le code
source seul ne permet pas d’affirmer le nombre de branches exécutées.

La sélection pourrait plus tard être faite à un niveau supérieur par templates
ou par plusieurs versions de recherche. On évite cette complexité pour l’instant :
le microbenchmark mesure déjà l’API réellement livrée, avec ses tests et appels.

## Qui attaque une case ?

attackersTo renvoie les cases d’origine des attaquants de la couleur demandée,
pas leurs destinations. On projette les attaques depuis la cible, puis on les
intersecte avec les ensembles de pièces de Position. Les rayons et les déplacements
de cavalier/roi sont symétriques. Pour les pions, il faut inverser la couleur :
les origines des pions blancs attaquant d4 sont les cases attaquées par un pion
noir imaginaire en d4, soit c3 et e3.

```cpp
#include "board/position.hpp"
#include "board/attacks.hpp"

const chess::Position position;
const chess::attacks::SlidingAttacks sliding;
const bool checked = chess::attacks::inCheck(position, chess::Color::White, sliding);
```

isSquareAttacked teste si cet ensemble est non vide ; inCheck utilise la case du
roi. Ces fonctions consultent la position actuelle. Pour valider un futur déplacement
de roi, il faudra considérer l’occupation après le coup : le roi peut libérer une
ligne auparavant bloquée. On ne peut pas filtrer tous ses coups uniquement avec
une carte d’attaques calculée sur le plateau initial.

## Tests et mesures

Les treize suites du projet passent avec GCC 15 en Debug et Release. Elles passent
aussi avec Clang 21, PEXT désactivé, AddressSanitizer et UndefinedBehaviorSanitizer.
LeakSanitizer a nécessité une exécution hors du sandbox, dont le traçage empêchait
son fonctionnement ; la relance complète a réussi sans rapport d’erreur.

Les quatre suites d’attaques couvrent :

- toutes les cases pour les pions, cavaliers et rois, contre un oracle en coordonnées ;
- les rayons vides, pleins, bloqués et quelques masques numériques explicites ;
- les 107648 occupations pertinentes, vérifiées contre un oracle qui examine les
  destinations et les cases entre les extrémités, indépendamment du parcours de production ;
- les mêmes occupations avec tous les bits non pertinents positionnés, puis
  8192 occupations pseudo-aléatoires complètes et des requêtes de dame ;
- le repli sans PEXT, les cases défendues, l’inversion des pions, les pièces clouées,
  les blocages, les attaques multiples et les rois adjacents.

Une erreur de fixture a été corrigée pendant le développement : un test de sens
d’attaque de pion plaçait son roi assez près pour attaquer lui-même la case testée.
Les rois ont été éloignés pour isoler la propriété voulue.

Le microbenchmark se compile séparément :

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DCHESS_BUILD_BENCHMARKS=ON
cmake --build build-release --config Release
./build-release/attack_bench
```

Il utilise 4096 couples case/occupation déterministes, répétés 128 fois après un
échauffement. Une requête calcule tour | fou. L’initialisation est chronométrée
séparément ; une somme de contrôle affichée vérifie l’accord des méthodes et
rend le résultat observable. Les temps ne sont pas des assertions de test.

Exécution locale GCC Release, 524288 requêtes par méthode :

| Méthode | Initialisation | Temps de requêtes | Millions de requêtes/s |
|---|---:|---:|---:|
| Rayons | < 0,001 ms | 35,36 ms | 14,83 |
| Tables portables | 2,45 ms | 12,09 ms | 43,38 |
| Tables PEXT | 2,34 ms | 2,29 ms | 229,28 |

La somme commune était `7fdcf4b7f55f2500`. Ce résultat porte sur une exécution,
un corpus synthétique souvent dense et un ensemble réutilisé susceptible de
rester en cache. Il ne mesure ni NPS de recherche, ni Elo, ni le coût en concurrence
avec une TT. Aucun backend n’est déclaré universellement supérieur sur cette base.
Le benchmark complet du moteur et le profiling restent prévus aux phases suivantes.

Pour forcer les méthodes portables :

```sh
cmake -S . -B build-no-pext -DCMAKE_BUILD_TYPE=Debug -DCHESS_DISABLE_PEXT=ON
cmake --build build-no-pext --config Debug
ctest --test-dir build-no-pext -C Debug --output-on-failure
```

## Exercices facultatifs

1. Ajouter les attaques d’un ensemble de pions avec les décalages diagonaux de
   bitboard. Comparer le résultat à l’union des attaques individuelles pour chaque
   singleton, les fichiers a/h et plusieurs ensembles. Justifier les masques de bord.
2. Dessiner le masque pertinent d’une tour en a1 et d’un fou en d4. Expliquer
   pourquoi la dernière case d’un rayon disparaît du masque mais reste dans l’attaque.
3. Écrire un test montrant qu’enlever un obstacle peut révéler une attaque sur un
   roi. Expliquer pourquoi cela compte pour make/unmake et les captures en passant.
4. Modifier uniquement le corpus du benchmark pour distinguer occupations denses,
   clairsemées et issues de FEN. Formuler une hypothèse sur les branches et le cache,
   puis comparer les mesures sans en déduire une vitesse de recherche.

Avant la phase 4, pouvoir expliquer : premier obstacle inclus, pion inversé dans
attackersTo, attaques d’une pièce clouée, masque pertinent et durée de vie des tables.
