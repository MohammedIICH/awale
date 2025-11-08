# Nom de l'exécutable
EXEC = testerJeu

# Fichiers sources
SRC = testerJeu.c jeu.c

# Compilateur
CC = gcc
CFLAGS = -Wall -Wextra -g

# Règle par défaut
all: $(EXEC)
	@echo "Compilation terminée. Lancement du test..."
	@./$(EXEC)

# Compilation
$(EXEC): $(SRC)
	$(CC) $(CFLAGS) -o $(EXEC) $(SRC)

# Nettoyage
clean:
	rm -f $(EXEC)