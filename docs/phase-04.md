# Phase 4 — génération pseudo-légale et MoveList

## Architecture et fichiers

Le générateur transforme les attaques et l’état d’une Position en descriptions
Move. Il consulte Position et SlidingAttacks par référence constante, puis remplit
une liste fournie par l’appelant. Il ne modifie pas le plateau.

- `src/board/move_list.hpp` : stockage contigu de coups, capacité fixe.
- `src/movegen/movegen.hpp` : contrat de generatePseudoLegal.
- `src/movegen/movegen.cpp` : pions, pièces, promotions, en passant et roques.
- `tests/movegen_tests.cpp` : six suites et un oracle indépendant.
- `src/main.cpp` : commande de diagnostic --pseudo-moves.
- `CMakeLists.txt` : compilation et enregistrement CTest.

```cpp
#include "movegen/movegen.hpp"

const chess::Position position;
const chess::attacks::SlidingAttacks sliding;
chess::MoveList moves;
chess::generatePseudoLegal(position, sliding, moves);
for (const auto move : moves) {
    // Consommer la description, sans supposer encore sa légalité.
}
```

Une seconde génération remplace le contenu de la liste. Aucun état de recherche
ni MovePicker n’est nécessaire à cette phase.

## Frontière entre pseudo-légalité et légalité

Les coups ordinaires respectent la géométrie, les obstacles, le trait et les règles
locales de leur pièce. Ils ne capturent ni une pièce amie ni le roi adverse.
Ils peuvent encore laisser leur propre roi en échec : pièce clouée déplacée,
roi entrant sur une case attaquée ou prise en passant découvrant une ligne.

Le roque a un traitement particulier : ses droits, ses cases libres et la sécurité
du roi sur sa case initiale, de passage et d’arrivée sont déjà vérifiés. Cette
convention doit être connue des appelants ; « pseudo-légal » n’a pas à signifier
que tous les types de coups omettent exactement les mêmes contrôles.

La roadmap place make/unmake en phase 5. Le filtrage général y sera ajouté :
faire le coup, vérifier que le roi du camp ayant joué n’est pas attaqué, puis
annuler le coup. Aucun faux generateLegal ne retourne la liste actuelle. Les
perft et leurs références officielles de génération attendent la phase 6 ; le
nombre 20 de la position initiale est ici un test de racine, pas une validation perft.

## MoveList : capacité et invariants

MoveList contient std::array<Move, 512> et un compteur uint16_t. Seul le préfixe
[0, size) appartient à la liste. clear remet le compteur à zéro sans parcourir les
éléments. Le tableau est initialisé lors de la construction : son coût n’est donc
pas nul, même si réutiliser une liste avec clear est constant.

Pourquoi 512 plutôt que s’appuyer sur un maximum usuel de coups légaux ? Notre
Position accepte des positions structurellement cohérentes sans prouver qu’elles
sont atteignables, et nous générons des coups pseudo-légaux. Une borne conservatrice
se déduit des invariants présents :

- au plus 16 pièces du camp au trait, dont exactement un roi ;
- une dame a au plus 27 destinations ; une tour 14, un fou 13, un cavalier 8 ;
- un pion a au plus 12 descriptions en promotion (trois destinations × quatre
  choix) ; en dehors de la promotion, son nombre est inférieur ;
- le roi a au plus huit déplacements et deux roques.

Ainsi `15 × 27 + 8 + 2 = 415` borne le nombre de descriptions, sans prétendre
que ces maxima puissent coexister. La capacité 512 couvre cette borne et ne dépend
pas d’une hypothèse sur le nombre de dames promues. Elle devra être revue si l’on
accepte d’autres variantes ou davantage de pièces.

tryPush renvoie false sur une liste pleine ou un Move absent, sans la modifier.
push est destiné au générateur : une violation entraîne abort, également en
Release, au lieu d’écrire hors limites ou de tronquer silencieusement les coups.
Ce chemin signale une erreur de programmation, impossible avec la borne et les
préconditions actuelles. Les doublons ne sont pas interdits par le conteneur :
c’est le générateur qui doit ne produire chaque description qu’une fois.

L’accès indexé exige i < size et le vérifie par assert en Debug. Les itérateurs et
view() exposent seulement le préfixe actif, en lecture seule. L’API de modification
pour le tri sera conçue avec le move ordering, quand son contrat sera nécessaire.

## Layout mémoire et lifetime

Sur l’ABI Linux x86-64 utilisée ici :

```text
MoveList, sizeof = 1026, alignof = 2
  0..1023     512 Move × 2 octets
1024..1025    compteur uint16_t
```

MoveList ne possède aucun pointeur ni allocation distante. La taille du tableau
est fixe et ses coups sont contigus. Sur une machine à lignes de cache de 64 octets,
32 Move tiennent dans une ligne s’ils sont correctement alignés ; l’alignement de
2 ne garantit pas que la première case commence sur une telle frontière.

Le compteur est après la réserve de coups : il est distant des premières entrées.
Le placer avant le tableau pourrait favoriser les listes courtes, mais décalerait
les données ; aucun changement de layout n’est fait sans mesure. La réserve vaut
environ 1 Kio même si la position n’offre que 20 coups. Une capacité de 256 réduirait
ce coût mais demanderait une autre preuve ou un traitement du dépassement.

Un vector aurait une taille variable, avec allocation/capacité à gérer. Une réserve
préallouée pourrait convenir, mais std::array rend la limite et le coût explicites.
Une structure de recherche possédant une liste par ply devra compter cette mémoire :
128 listes représenteraient 131328 octets sur cette ABI, avant le reste de la pile.
Ce n’est pas encore le layout de SearchStack, qui sera conçu ultérieurement.

Une copie de MoveList copie sa réserve entière, pas seulement ses coups actifs.
Le générateur reçoit donc la destination par référence. Les span et pointeurs
retournés ne prolongent pas sa durée de vie ; ils ne doivent pas survivre à la
liste. clear ou une nouvelle génération rendent leur ancien contenu logique
obsolète, même si l’adresse mémoire reste identique. Les valeurs Move copiées,
en revanche, sont autonomes.

## Pions et encodages spéciaux

Pour chaque pion, la direction vaut +1 rang pour les blancs, -1 pour les noirs.
La poussée simple exige une case vide. La double poussée est examinée seulement
si la simple est possible, depuis le rang initial, et si l’arrivée est aussi vide.
Une double poussée ne saute donc jamais un obstacle.

Les captures sont les attaques diagonales intersectées avec les pièces adverses,
roi exclu. Arriver sur le dernier rang produit quatre Move distincts : cavalier,
fou, tour et dame. Cela s’applique aux captures comme aux poussées. Le générateur
ne produit jamais un coup calme sans promotion sur le dernier rang.

La cible en passant est traitée séparément des captures ordinaires : son arrivée
est vide, mais son flag indique une capture. Position garantit déjà la présence
du pion adverse ayant poussé et la cohérence de la cible. Deux pions peuvent
parfois capturer cette même cible ; chacun donne un coup distinct.

Exemple `k7/8/8/r4pPK/8/8/8/8 w - f6 0 1` : g5f6 en passant est produit. Après
retrait des pions f5/g5, la tour a5 attaquerait le roi h5. Ce candidat sera rejeté
par le filtrage légal de phase 5. Vérifier seulement l’arrivée du pion serait faux.

## Cavaliers, fous, tours, dames et roi

Le générateur parcourt les bitboards de pièces en retirant leur bit de poids faible.
Pour chaque origine, il obtient ses attaques puis retire les pièces amies et le
roi adverse. Le roi adverse reste dans l’occupation transmise aux attaques
glissantes : on ne peut donc pas passer à travers lui.

Chaque destination restante donne un coup Capture si elle contient une pièce
adverse, Quiet sinon. Le switch sur le type reste simple et inspectable ; il se
trouve dans une boucle dont le type reste stable sur toutes les pièces concernées.
Une spécialisation par templates serait possible, au prix de davantage de code
et de pression sur le cache d’instructions. Nous n’en avons pas encore besoin.

## Roques orthodoxes

Les droits viennent de GameState, jamais d’une simple inspection du plateau.
Le roi et la tour doivent être sur leurs cases initiales. Pour le petit roque,
f/g doivent être libres ; pour le grand, b/c/d doivent être libres. Le roi ne
traverse que f ou d et termine en g ou c.

On interdit le roque si la case initiale, la case de passage ou l’arrivée du roi
est attaquée. Une attaque sur la tour ou sur b1/b8 seule ne l’interdit pas. Les
tests distinguent ces conditions pour éviter de traiter tout le chemin de la
tour comme un chemin du roi.

Ici, les requêtes portent sur l’occupation actuelle. Pour ces roques orthodoxes,
cela suffit après avoir exclu un échec sur la case initiale : une attaque glissante
sur le passage ou l’arrivée qui serait masquée par le roi initial attaquerait déjà
cette case initiale. Les cases entre roi et tour sont libres et le déplacement
de la tour ne révèle pas une ligne passant derrière son coin du plateau. Ce
raisonnement particulier ne s’étend pas aux coups ordinaires de roi ni à Chess960.

Les roques produisent le déplacement du roi, avec KingCastle ou QueenCastle ;
le déplacement de la tour sera réalisé dans makeMove.

## Complexité, allocations et branches

Soit P le nombre de pièces du camp au trait et M le nombre de coups produits.
La génération coûte O(P + M) avec des consultations d’attaques constantes ; avec
les rayons, on ajoute leurs cases parcourues, et avec l’extraction logicielle,
les bits de masque examinés. Sur 64 cases tous ces facteurs sont bornés.

Aucune allocation dynamique, exception ou copie de Position n’a lieu dans
generatePseudoLegal. SlidingAttacks est construit à l’extérieur : sa variante
indexée peut allouer une fois, pas à chaque génération. Le code n’est pas récursif.
L’assertion de cohérence ajoute une validation complète en Debug ; elle disparaît
en Release, où la cohérence reste une précondition de l’API.

Les boucles de bits effectuent un tour par pièce ou destination présente. Les
branches sur les captures varient avec la position ; celles sur la couleur et
le type sont plus stables dans leurs boucles. La branche de capacité de push
est toujours non prise dans le générateur correct. La génération des pions est
volontairement individuelle pour rendre les règles lisibles ; une version par
ensembles décalés pourra être comparée plus tard avec des mesures.

L’ordre livré est déterministe, mais n’a aucune valeur tactique : pions d’abord,
puis types de pièces, puis roques, avec parcours croissant des bits. Les promotions
sont produites cavalier/fou/tour/dame. Ne pas confondre cet ordre de construction
avec le futur move ordering de recherche.

## Tests et inspection CLI

Les 19 suites passent sous Linux avec GCC 15, en Debug et en Release, sans
avertissement de compilation. Windows n’a pas été exécuté pour cette phase.

Six nouvelles suites complètent les treize précédentes : liste, départ, pions,
roques, oracle et frontière pseudo-légale. Le conteneur est rempli jusqu’à sa
capacité pour vérifier le refus du dépassement et son remplacement par génération.
Les tests de départ vérifient les 16 poussées et quatre coups de cavalier de chaque
camp, soit 20 candidats chacun.

L’oracle ordinaire examine les 64 × 64 couples origine/destination, en coordonnées,
sans appeler les tables d’attaques. Il vérifie géométrie, cases intermédiaires,
flags et promotions. Les roques ont des fixtures explicites séparées. Les ensembles
de coups sont triés pour comparaison et contrôlés sans doublons ; les tests peuvent
allouer des vector, contrairement au générateur de production.

Les fixtures couvrent notamment promotions aux deux bords, les deux couleurs,
deux candidats en passant, pièces clouées et un plateau de nombreuses dames
structurellement accepté. Les backends rayons, tables portables et Auto doivent
produire exactement les mêmes descriptions. Position reste inchangée.

```sh
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
./build/chess_engine --pseudo-moves "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
```

La commande affiche explicitement qu’il s’agit de candidats pseudo-légaux, puis
leurs coordonnées longues, avec suffixe de promotion. Ce diagnostic n’est pas
encore une implémentation du protocole UCI. --fen conserve sa fonction précédente.

## Exercices facultatifs

1. Ajouter un test de promotion sur h2 pour les noirs, avec capture sur g1 et
   arrivée h1 libre. Énumérer les huit encodages attendus avant d’exécuter le test.
2. Écrire une fonction de test qui renverse verticalement une position et échange
   les couleurs. Vérifier la correspondance des coups, droits et cible en passant.
3. Produire un filtre des captures et promotions à partir d’une MoveList. Expliquer
   pourquoi tester seulement si l’arrivée est occupée omet l’en passant et pourquoi
   les promotions calmes sont utiles à la future quiescence.
4. Comparer sur papier le layout actuel à un compteur size_t et une capacité 256.
   Expliquer quelle propriété de correctness devrait justifier cette capacité.

Avant la phase 5, savoir distinguer attaque, candidat pseudo-légal et coup légal,
justifier la borne de MoveList et expliquer pourquoi l’en passant peut découvrir
un échec sans déplacer le roi.
