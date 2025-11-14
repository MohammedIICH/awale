############################################
#                 Makefile                 #
############################################

CC      = gcc
CFLAGS  = -Wall -Wextra -Wpedantic -std=c11 -O2
LDFLAGS = 

# Répertoires
SRV_DIR = server
CLI_DIR = client
GAME_DIR = game
BIN_DIR = bin
OBJ_DIR = obj

# ================================
#         Sources Serveur
# ================================
SERVER_SRC = \
    $(SRV_DIR)/server_main.c \
    $(SRV_DIR)/server_accounts.c \
    $(SRV_DIR)/server_games.c \
    $(SRV_DIR)/server_utils.c \
    $(GAME_DIR)/game.c

# ================================
#         Sources Client
# ================================
CLIENT_SRC = \
    $(CLI_DIR)/client.c \
    $(CLI_DIR)/client_ui.c \
    $(CLI_DIR)/client_protocol.c \
    $(CLI_DIR)/client_utils.c \
    $(GAME_DIR)/game.c

# ================================
#         Objets
# ================================
SERVER_OBJ = $(SERVER_SRC:%.c=$(OBJ_DIR)/%.o)
CLIENT_OBJ = $(CLIENT_SRC:%.c=$(OBJ_DIR)/%.o)

# Binaries
SERVER_BIN = $(BIN_DIR)/server
CLIENT_BIN = $(BIN_DIR)/client

# Règle par défaut
all: prepare $(SERVER_BIN) $(CLIENT_BIN)

############################################
#           Compilation server
############################################

$(SERVER_BIN): $(SERVER_OBJ)
	$(CC) $(CFLAGS) $(SERVER_OBJ) -o $@ $(LDFLAGS)
	@echo "Server built → $@"

############################################
#           Compilation client
############################################

$(CLIENT_BIN): $(CLIENT_OBJ)
	$(CC) $(CFLAGS) $(CLIENT_OBJ) -o $@ $(LDFLAGS)
	@echo "Client built → $@"

############################################
#      Compilation générique des .o
############################################

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

############################################
#       Préparation des dossiers
############################################

prepare:
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(OBJ_DIR)/client
	@mkdir -p $(OBJ_DIR)/server
	@mkdir -p $(OBJ_DIR)/game

############################################
#              Nettoyage
############################################

clean:
	rm -rf $(OBJ_DIR)

mrproper: clean
	rm -rf $(BIN_DIR)

############################################

.PHONY: all clean mrproper prepare run-server run-client
