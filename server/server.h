/*************************************************************************
                           Awale -- Game (Server)
                             -------------------
    début                : 20/10/2025
    auteurs              : Mohammed Iich et Dame Dieng
    e-mails              : mohammed.iich@insa-lyon.fr et dame.dieng@insa-lyon.fr
    description          : Déclarations globales du serveur réseau Awalé :
                           - gestion des comptes persistants
                           - gestion des clients connectés
                           - gestion des parties en cours
                           - API utilitaire partagée
*************************************************************************/

#ifndef SERVER_H
#define SERVER_H

#include <sys/select.h>
#include "../game/game.h"

/* ================================================================
 *  Constantes globales
 * ================================================================ */

#define MAX_CLIENTS   20
#define MAX_GAMES     (MAX_CLIENTS / 2)
#define MAX_ACCOUNTS  256
#define BUF_SIZE      512
#define DEFAULT_PORT  4444

/* Fichier où sont stockés tous les comptes */
#define USERS_FILE    "users/accounts.txt"

/* ================================================================
 *  Structures de données
 * ================================================================ */

/*
 * Compte utilisateur persistant :
 *  username : pseudo unique (case-insensitive pour login)
 *  password : stocké en clair (choix simplifié pour projet)
 *  bio      : texte multi-lignes (nettoyé et limité)
 *  friends  : liste de pseudos séparés par des virgules
 */
typedef struct {
    char username[16];
    char password[32];
    char bio[512];
    char friends[256];
} Account;

/*
 * Client connecté au serveur :
 *  fd            : socket TCP
 *  name          : pseudo authentifié
 *  logged_in     : connexion authentifiée
 *  in_game       : joueur actuellement dans une partie
 *  opponent_index: index de l'adversaire (g_clients[])
 *  player_index  : 0 ou 1 (position dans la partie)
 *  ready         : a envoyé READY
 *  login_stage   : 0=username, 1=password, 2=authentifié
 *  private_mode  : parties observables uniquement par amis
 *  pending_friend_reqs : demandes d'amis en attente (nom,nom,...)
 */
typedef struct {
    int  fd;
    char name[16];
    int  logged_in;
    int  in_game;
    int  opponent_index;
    int  player_index;
    int  ready;
    int  login_stage;
    int  private_mode;
    char pending_friend_reqs[256];
} Client;

/*
 * Session de jeu Awalé :
 *  active         : partie en cours
 *  board[12]      : plateau
 *  p0, p1         : joueurs (struct Player du moteur game/)
 *  to_move        : 0 ou 1 → joueur à jouer
 *  filename       : fichier de log
 *  player_fd0/1   : sockets des 2 joueurs
 *  observers[]    : sockets des observateurs
 *  observer_count : nb d'observateurs
 */
typedef struct {
    int    active;
    int    board[12];
    Player p0;
    Player p1;
    int    to_move;
    char   filename[160];
    int    player_fd0;
    int    player_fd1;
    int    observers[MAX_CLIENTS];
    int    observer_count;
} Game;

/* ================================================================
 *  Données globales
 * ================================================================ */

extern Client  g_clients[MAX_CLIENTS];
extern Game    g_games[MAX_GAMES];
extern Account g_accounts[MAX_ACCOUNTS];
extern int     g_account_count;

extern fd_set  g_master_set;
extern int     g_max_fd;

/* ================================================================
 *  API principale du serveur
 * ================================================================ */

void server_run(int port);

/* Connexion / déconnexion */
void server_handle_new_connection(int server_fd);
void server_handle_client_message(int fd);
void server_remove_client(int fd);

/* ================================================================
 *  Gestion des comptes
 * ================================================================ */

int         accounts_load(const char *path);
void        accounts_save(const char *path);
int         accounts_find(const char *username);
int         accounts_is_friend(int acc_index, const char *username);
void        accounts_set_bio(int acc_index, const char *bio);
const char *accounts_get_bio(int acc_index);
void        accounts_add_friend(int acc_index, const char *friend_name,
                                int *ok_added, int *full);
int         accounts_remove_friend(int acc_index, const char *friend_name);

/* ================================================================
 *  Fonctions utilitaires
 * ================================================================ */

/* Envoie un message à tous sauf except_fd (ou -1). */
void server_broadcast(const char *msg, int except_fd);

/* Recherche client par pseudo (case-insensitive). */
int  client_index_by_name(const char *name);

/* Teste si username est déjà connecté. */
int  username_logged_in(const char *username);

/* Compare deux chaînes en ignorant la casse. */
int  ci_equal(const char *a, const char *b);

/* Convertit une chaîne en minuscules. */
void to_lowercase(char *dst, const char *src, size_t max);

/* safe string operations */
void safe_strcpy(char *dst, const char *src, size_t dstsz);
void safe_strcat(char *dst, const char *src, size_t dstsz);
size_t strlcat_safe(char *dst, const char *src, size_t dstsz);
void copy_bounded(char *dst, size_t dstsz, const char *src);
void append_bounded(char *dst, size_t dstsz, const char *src);
size_t strlcpy_safe(char *dst, const char *src, size_t dstsz);

/* ================================================================
 *  Gestion des parties (game sessions)
 * ================================================================ */

void games_send_board(Game *g);
int  games_start(int client_a, int client_b);
void games_process_move(int client_index, int pit);
int  games_find_by_player_name(const char *name);
int  games_can_observe(Game *g, const char *observer_name);
int  games_add_observer(Game *g, int fd);
void games_remove_observer_fd(int fd);
void games_cancel_by_client(int client_index, int notify);

#endif /* SERVER_H */
