# Awale Game

Un jeu Awale en réseau implémenté en C avec une architecture client-serveur.

## Description

Awale est un ancien jeu de stratégie africain. Ce projet implémente une version jouable avec une interface utilisateur interactive et un système de gestion multi-clients.

## Architecture

```
awale/
├── client/                 # Code client
│   ├── client.c           # Point d'entrée client
│   ├── client.h           # Déclarations client
│   ├── client_ui.c        # Interface utilisateur
│   ├── client_protocol.c  # Gestion du protocole
│   └── client_utils.c     # Fonctions utilitaires
├── server/                # Code serveur
│   ├── server_main.c      # Point d'entrée serveur
│   ├── server.h           # Déclarations serveur
│   ├── server_accounts.c  # Gestion des comptes
│   ├── server_games.c     # Gestion des jeux
│   └── server_utils.c     # Fonctions utilitaires
├── game/                  # Logique du jeu
│   ├── game.c             # Implémentation du jeu
│   └── game.h             # Déclarations du jeu
└── Makefile              # Configuration de compilation
```

## Démarrage rapide

### Prérequis

- Compilateur C (gcc, clang)
- Make
- POSIX-compliant système (Linux, macOS, Unix)

### Compilation

```bash
# Compiler les exécutables
make

# Ou compiler un composant spécifique
make client
make server
```

### Utilisation

1. **Démarrer le serveur** :
```bash
# Lancer sur le port par défaut (4444)
./server

# Ou spécifier un port personnalisé
./server <port>
```

2. **Lancer le client** :
```bash
./client <host> <port>
```

3. **Arrêter les services** :
```bash
make clean
```

## Règles du jeu

Awale est un jeu de stratégie pour deux joueurs :

- **Plateau** : 12 cases (6 par joueur) + 2 réserves
- **Objectif** : Capturer plus de graines que l'adversaire
- **Mécanique** : Les joueurs distribuent les graines en avançant sur le plateau

## Structure des fichiers

### Client
- `client.c` : Boucle principale et gestion des connexions
- `client_ui.c` : Affichage du plateau et interface utilisateur
- `client_protocol.c` : Sérialisation/désérialisation des messages
- `client_utils.c` : Utilitaires pour le traitement des entrées

### Serveur
- `server_main.c` : Acceptation des connexions et gestion des clients
- `server_accounts.c` : Authentification et profils utilisateur
- `server_games.c` : Création et gestion des parties
- `server_utils.c` : Fonctions utilitaires

### Logique du jeu
- `game.c` : Implémentation des règles du jeu Awale
- `game.h` : Structures de données et prototypes

## Compilation détaillée

```bash
# Compiler avec flags de débogage
make DEBUG=1

# Compiler en mode optimisé
make OPTIMIZE=1

# Voir les commandes exécutées
make VERBOSE=1
```

## Notes de développement

- Le projet utilise les sockets POSIX pour la communication réseau
- Protocole personnalisé basé sur des messages texte/binaires
- Gestion multi-clients avec select() ou threads

## Auteur

**Mohammed IICH & Dame Dieng**
- Repository: [awale](https://github.com/MohammedIICH/awale)

---

**Dernière mise à jour** : Novembre 2025
