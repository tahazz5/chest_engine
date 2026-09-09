# Jouer maintenant

## Lancer une partie

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --config Release
./build-release/chess_engine --play
```

Tu joues les blancs contre un adversaire simple. Pour les noirs, ajouter `black` ;
pour deux personnes sur le même terminal, ajouter `local`. Avec Visual Studio,
l’exécutable se trouve habituellement dans `build-release\Release\chess_engine.exe`.

Le plateau utilise P/N/B/R/Q/K pour pion/cavalier/fou/tour/dame/roi. Les blancs sont
en majuscules, les noirs en minuscules. L’affichage est retourné lorsque tu choisis
les noirs.

| Saisie | Action |
|---|---|
| e2e4 | Jouer de e2 vers e4 |
| e1g1 / e1c1 | Petit / grand roque blanc, si légal |
| a7a8q | Promotion en dame ; n/b/r pour cavalier/fou/tour |
| coups | Afficher les coups légaux disponibles |
| fen | Afficher la position actuelle |
| undo | Annuler ton coup et la réponse ; un seul coup en mode local |
| aide | Rappeler les commandes |
| abandon | Abandonner la partie |
| quit | Fermer la partie |

Les coups sont saisis en coordonnées longues, pas en notation SAN. Une promotion
exige son suffixe : l’interface ne choisit pas une dame silencieusement. Une entrée
invalide ne modifie pas le plateau. La fermeture du flux d’entrée quitte proprement.

## Périmètre ajouté

Pour rendre le projet jouable à la demande, cette étape ajoute les fondations
make/unmake et légalité de phase 5, un premier verrou perft, et un adversaire de
démonstration. Elle ne prétend pas terminer toutes les phases restantes de la roadmap.
Le moteur avancé, son évaluation complète, la quiescence, les tables de transposition,
la gestion du temps et UCI restent à construire pédagogiquement.

Les fichiers sont :

- `src/board/position.hpp/.cpp` : UndoState, makeMove et unmakeMove.
- `src/movegen/movegen.hpp/.cpp` : generateLegal.
- `src/movegen/perft.hpp` : comptage récursif de contrôle.
- `src/play/play.hpp/.cpp` : affichage, interaction, historique et adversaire simple.
- `tests/legal_tests.cpp` : mutations, annulations, légalité et perft.
- `tests/play_tests.cpp` : parties scriptées, fins de partie et choix de l’adversaire.

chess_play dépend de chess_core. Le cœur ne dépend ni des flux du terminal ni de
l’interface. Le point d’entrée play accepte des flux, ce qui permet de le tester
avec des chaînes et de rejouer des commandes de manière déterministe.

## Make/unmake : invariants et disposition mémoire

makeMove exige un coup issu du générateur pseudo-légal de la position courante.
Il n’est pas un parseur de coups arbitraires et ne rejette pas lui-même l’auto-échec.
La saisie utilisateur passe uniquement par la liste légale, avant toute mutation.

Le plateau est modifié à travers putPiece/removePiece : tableau de cases et
bitboards restent synchronisés. Une capture en passant retire une pièce ailleurs
qu’à l’arrivée ; un roque déplace aussi la tour ; une promotion remplace le pion.
Les droits de roque disparaissent quand le roi bouge, quand une tour quitte son
coin ou quand ce coin est capturé. Une ancienne cible en passant expire au coup
suivant. Le compteur de demi-coups est remis à zéro sur pion/capture, et le numéro
de coup avance après les noirs. Les compteurs saturent à UINT32_MAX plutôt que
déborder à zéro.

UndoState contient le GameState antérieur et la pièce capturée. Le coup lui-même
permet de retrouver la case de capture en passant et de reconnaître une promotion
ou un roque. Le type mobile normal est encore disponible sur l’arrivée avant
annulation. Il n’est donc pas nécessaire de copier les 144 octets de Position.

Sur l’ABI x86-64 actuelle, GameState occupe 12 octets ; avec une Piece d’un octet
et le padding final, UndoState occupe 16 octets, alignés sur 4. Aucun pointeur
n’est conservé. Les informations d’annulation sont des valeurs propriétaires.
Les undo doivent correspondre exactement aux coups joués, dans l’ordre inverse.
Un undo ne doit pas être réutilisé sur une autre branche de partie.

La complexité de make/unmake est O(1) : nombre fixe de cases et de métadonnées
actualisées. Les assertions Debug vérifient la cohérence complète ; leur parcours
de 64 cases ne fait pas partie du chemin Release. Les branches spéciales (promotion,
roque, en passant) sont rares en début de partie, mais aucune hypothèse de vitesse
n’est déduite de cette fréquence sans mesure.

Zobrist n’est pas encore introduit : il faudra ajouter ses mises à jour et ses
invariants lors de la phase dédiée. Aucun hash nul n’est utilisé comme identité.

## Filtrage légal et perft

Pour chaque candidat, generateLegal fait le coup, teste le roi du camp ayant joué,
puis annule exactement le coup. Le trait a déjà changé lors du test : il faut
explicitement mémoriser la couleur initiale. La position fournie est restaurée
avant le retour ; elle ne doit pas être consultée simultanément par un autre thread
pendant cette opération.

Le filtre utilise une liste temporaire fixe, sans allocation. Son coût est O(M)
mutations et requêtes d’échec pour M candidats, en plus de la génération. Cette
méthode simple sert de référence avant d’envisager un traitement direct des clouages.
Les conditions spécifiques du roque sont déjà vérifiées par le générateur.

perft compte les chemins de coups légaux à profondeur fixée. Il ignore volontairement
les règles de nulle et ne sert pas de recherche de meilleur coup. Le coût est
exponentiel en profondeur, avec une mémoire de pile proportionnelle à celle-ci.
La profondeur est une précondition non négative ; les tests utilisent de petites
valeurs dont les résultats tiennent dans uint64_t. Aucun divide n’est livré ici.

Le verrou perft a passé avant l’ajout de l’adversaire :

| Position | Profondeur | Résultat |
|---|---:|---:|
| Départ | 1 / 2 / 3 / 4 | 20 / 400 / 8902 / 197281 |
| Kiwipete | 3 | 97862 |
| Finale standard, position 3 | 4 | 43238 |
| Position standard 4 | 3 | 9467 |
| Position standard 5 | 3 | 62379 |

Références : [Perft Results](https://www.chessprogramming.org/Perft_Results).
Les tests vérifient également le retour exact à la position initiale après le comptage.

## Adversaire et gestion de partie

L’adversaire explore deux demi-coups (son coup et une réponse), avec une évaluation
du matériel et un petit bonus central pour pions/cavaliers/fous. Il distingue mat
et pat, y compris aux feuilles. Il est déterministe, sans alpha-beta, quiescence,
TT ni gestion de temps. Il peut donc commettre des erreurs tactiques et stratégiques.
Ce module rend le jeu possible ; il ne remplace pas le futur moteur de recherche.

La recherche utilise make/unmake et des MoveList locales. Son coût est environ
quadratique en nombre de coups sur deux demi-coups, plus la génération aux feuilles.
Il n’y a aucune allocation par nœud. Les tables d’attaques sont construites une
fois par session et restent immuables. Les listes contiguës favorisent les parcours
séquentiels ; les futurs profils devront mesurer leur interaction avec les tables.

L’interface conserve un vector des coups/undo et un vector de clés de répétition.
Ces allocations ont lieu à l’échelle d’une partie, hors recherche. La clé textuelle
inclut plateau, trait, droits de roque et cible en passant seulement si une prise
légale existe ; les compteurs sont exclus. L’annulation retire aussi ces clés.
Ce stockage privilégie ici la transparence ; un historique de hash le remplacera
plus tard avec Zobrist.

Le mode reconnaît mat, pat, trois répétitions, 50 coups sans capture ni mouvement
de pion et les cas simples de matériel insuffisant (rois seuls, une seule pièce
mineure, ou uniquement des fous sur cases de même couleur). Il ne prétend pas
résoudre toutes les positions mortes théoriques. Trois répétitions et 50 coups
sont revendiqués automatiquement dans ce mode, comme annoncé au lancement ; ce
n’est pas une interface complète de réclamations d’arbitrage. L’adversaire ne
modélise pas l’historique des répétitions dans son évaluation à deux demi-coups.

## Validation et exercices facultatifs

Les tests scriptés couvrent coup invalide, réponse de l’ordinateur, annulation,
retour au FEN initial, promotion, choix des noirs, EOF, mat du sot, pat, répétitions,
50 coups et un mat en un trouvé par l’adversaire. Les tests du cœur parcourent
les coups spéciaux et vérifient leur annulation ainsi que les perft de référence.

1. Ajouter une vérification explicite des droits après une capture de tour sur
   son coin, puis vérifier leur restauration par unmakeMove.
2. Suivre sur papier make/unmake d’une promotion avec capture : où retrouver le
   pion et la pièce capturée sans recopier Position ?
3. Ajouter divide(depth) à partir du perft actuel et vérifier que la somme des
   branches correspond au total.
4. Rejouer une courte partie avec undo, puis comparer le FEN au précédent. Pourquoi
   faut-il annuler aussi l’historique de répétition ?
