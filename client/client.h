/*******************************************************************************
                           Awale -- Client
                             -------------------
    début                : 20/10/2025
    auteurs              : Mohammed Iich et Dame Dieng
    e-mails              : mohammed.iich@insa-lyon.fr et dame.dieng@insa-lyon.fr
    description          : - Déclarations communes du client.
                           - Contient les structures d'état, constantes 
                             et prototypes utilisés par les différents modules.
********************************************************************************/

#ifndef CLIENT_H
#define CLIENT_H

#include <stddef.h>

/* Taille des buffers pour l'entrée / sortie réseau & clavier */
#define CLIENT_BUF_SIZE 1024

/* Quelques codes couleurs ANSI pour améliorer l'affichage */
#define COL_RESET   "\033[0m"
#define COL_RED     "\033[0;31m"
#define COL_GREEN   "\033[0;32m"
#define COL_YELLOW  "\033[0;33m"
#define COL_BLUE    "\033[0;34m"
#define COL_CYAN    "\033[0;36m"
#define COL_WHITE   "\033[0;37m"
#define COL_BOLD    "\033[1m"
#define COL_DIM     "\033[2m"

/*
 * Représente l'état local du client :
 *  - connexion réseau
 *  - informations de login
 *  - état de la partie et du mode observateur
 *  - dernier plateau reçu
 */
typedef struct {
    int  sock;                     /* socket connecté au serveur */

    /* Login / authentification */
    int  logged_in;                /* 1 si authentifié */
    int  expect_username;          /* en attente de saisie du pseudo */
    int  expect_password;          /* en attente de saisie du mot de passe */
    char pending_username[16];     /* pseudo en cours de saisie */
    char username[16];             /* pseudo définitif après login */

    /* État jeu / observation */
    int  in_game;                  /* 1 si joueur dans une partie */
    int  is_observer;              /* 1 si en mode spectateur */
    int  has_players;              /* 1 si noms des joueurs connus */

    char player0_name[16];         /* joueur "bas" du plateau */
    char player1_name[16];         /* joueur "haut" du plateau */
    int  player_index;             /* 0 ou 1 (côté du client dans la partie) */

    /* Dernier plateau reçu */
    int  board[12];
    int  score0;
    int  score1;
    int  next_player;              /* prochain joueur à jouer (0 ou 1) */

    int  board_pending;            /* 1 si un plateau est en attente d'affichage */
} ClientState;

/* ============================
 *  Fonctions utilitaires sûres
 * ============================ */

void safe_strcpy_client(char *dst, const char *src, size_t dstsz);
void safe_strinit(char *str, size_t sz);
int  safe_strcmp(const char *a, const char *b);
int  safe_strcasecmp(const char *a, const char *b);
int  is_string_empty(const char *str);
int  is_buffer_terminated(const char *buf, size_t size);
int  is_valid_board_index(int index);
int  is_valid_player_index(int index);
int  is_valid_score(int score);

/* ============================
 *  Interface de haut niveau
 * ============================ */

/*
 * Initialise l'état du client avec une socket connectée.
 * Met tous les champs à des valeurs cohérentes.
 */
void client_state_init(ClientState *state, int sock);

/*
 * Gère une ligne complète envoyée par le serveur.
 * Met à jour l'état (login, partie, observateur, etc.)
 * et appelle l'interface utilisateur pour rafraîchir l'affichage.
 */
void protocol_handle_server_line(ClientState *state, const char *line);

/* ============================
 *  Interface utilisateur (UI)
 * ============================ */

/* Efface l'écran du terminal (si compatible ANSI) */
void ui_clear_screen(void);

/* Affiche une séparation nette après la fin du login (transition vers le menu) */
void ui_show_login_success_screen(void);

/* Affiche une bannière d'accueil en haut du client */
void ui_print_banner(void);

/* Affiche la liste des commandes disponibles selon l'état courant */
void ui_print_commands(const ClientState *state);

/* Affiche le plateau Awalé + infos de tour / scores / mode */
void ui_print_board(const ClientState *state);

/* Affiche le prompt adapté à l'état (login / game / observer / idle) */
void ui_print_prompt(const ClientState *state);

/* ============================
 *  Boucle principale du client
 * ============================ */

/*
 * Point d'entrée de la logique client :
 *  - connexion TCP au serveur (ip, port)
 *  - boucle select() sur socket + stdin
 */
int client_run(const char *server_ip, const char *server_port);

#endif /* CLIENT_H */