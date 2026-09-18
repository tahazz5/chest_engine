# Chess Engine — C++20

Moteur d’échecs pédagogique en C++20, jouable dans le navigateur ou le terminal
contre un adversaire simple.

## Démarrage rapide : interface graphique

Prérequis : CMake >= 3.20, un compilateur C++20, Python 3 et un navigateur.
Aucun paquet Python supplémentaire n’est nécessaire. Depuis la racine du projet
sur Linux :

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --config Release
python3 ui/server.py
```

Ouvre ensuite [localhost:8000](http://localhost:8000) dans ton navigateur.
Laisse le serveur tourner pendant la partie ; `Ctrl+C` dans le terminal l’arrête.

- Tu joues les blancs contre l’ordinateur.
- Clique sur une pièce pour afficher ses destinations légales, puis sur une case
  marquée pour jouer. L’ordinateur répond automatiquement.
- **Undo turn** annule ton dernier coup et la réponse de l’ordinateur.
- **New game** remet l’échiquier à sa position initiale.
- Choisis la pièce dans **Promote pawn to** avant de jouer une promotion.

Cette interface est une démonstration locale : chaque tour rejoue l’historique
avec le moteur C++ pour calculer la position et les coups légaux. Actualiser la
page démarre une nouvelle partie. Le serveur attend l’exécutable
`build-release/chess_engine` ; compile-le avant de lancer l’interface.

## Jouer dans le terminal

```sh
./build-release/chess_engine --play
```

Tu joues les blancs. Saisis `e2e4`, `coups` pour les coups légaux, `undo` pour
annuler ton dernier tour ou `quit` pour quitter. Utiliser `--play black` pour les
noirs ou `--play local` pour jouer à deux. Le [guide de jeu](docs/mode-jouable.md)
explique les commandes, les limites de l’adversaire et les nouvelles fondations.

Pour construire la version de jeu :

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --config Release
```

## Compiler et tester

Prérequis : CMake >= 3.20 et compilateur C++20 (GCC, Clang ou MSVC).
Aucune dépendance externe ni téléchargement.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Sur Linux :

```sh
./build/chess_engine
./build/chess_engine --fen "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1"
```

Un FEN invalide produit un diagnostic sur stderr et un statut de sortie 1.
Avec un générateur Visual Studio, utiliser `build\Debug\chess_engine.exe`.
Pour Release, utiliser un autre dossier et remplacer Debug par Release.

## Apprendre par phases

- [Phase 1 : types, coups et bitboards](docs/phase-01.md).
- [Phase 2 : Position, invariants et FEN](docs/phase-02.md).
- [Phase 3 : attaques, rayons et tables PEXT](docs/phase-03.md).
- [Phase 4 : MoveList et coups pseudo-légaux](docs/phase-04.md).

Chaque chapitre explique les choix, les contrats, le layout mémoire, les coûts
et propose des exercices facultatifs. Le code de chaque phase livrée est complet.
Les descriptions de phases précédentes correspondent à leur étape ; la bibliothèque
est désormais statique et le harnais de tests est partagé dans test_support.hpp.

Un microbenchmark d’attaques est disponible avec `-DCHESS_BUILD_BENCHMARKS=ON` :
voir les commandes et limites d’interprétation dans le chapitre de phase 3.
`-DCHESS_DISABLE_PEXT=ON` permet de tester le repli portable.

Le [mode jouable](docs/mode-jouable.md) ajoute make/unmake, filtrage légal, premiers
perft et un adversaire de démonstration. Les coups affichés par `--pseudo-moves`
restent des candidats ; le jeu utilise exclusivement `generateLegal`.
Les phases avancées d’évaluation/recherche et UCI restent à développer.
