#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_LINE 1024  // TamaÃ±o mÃ¡ximo de la lÃ­nea de comando
#define MAX_ARGS 64    // NÃºmero mÃ¡ximo de argumentos
#define MAX_PATH 1024  // TamaÃ±o mÃ¡ximo del path
#define MAX_COMMANDS 100
#define MAX_FAVS 100  // Número máximo de comandos favoritos

typedef struct {
    int id;
    char command[MAX_LINE];  // Tamaño de línea que ya defines para los comandos
} Favorite;

Favorite favs[MAX_FAVS];  // Array para almacenar los favoritos
int fav_count = 0;  // Contador de favoritos actuales

char fav_file[MAX_PATH];  // Ruta del archivo de favoritos
int fav_file_created = 0; // Bandera para saber si el archivo ya fue creado

int is_favorite(const char *command) {
    for (int i = 0; i < fav_count; i++) {
        if (strcmp(favs[i].command, command) == 0) {
            return 1; // El comando ya está en la lista de favoritos
        }
    }
    return 0; // El comando no está en la lista de favoritos
}

void show_favorites() {
    if (fav_count == 0) {
        printf("No hay comandos en la lista de favoritos.\n");
        return;
    }

    printf("Lista de comandos favoritos:\n");
    for (int i = 0; i < fav_count; i++) {
        printf("%d. %s\n", favs[i].id, favs[i].command);  // Mostrar el número y el comando
    }
}

void clear_favorites() {
    fav_count = 0;  // Reiniciar el contador de favoritos
    printf("Todos los comandos favoritos han sido eliminados.\n");
}

void search_favorites(const char *substring) {
    int found = 0;
    
    printf("Comandos que contienen '%s':\n", substring);
    
    for (int i = 0; i < fav_count; i++) {
        if (strstr(favs[i].command, substring) != NULL) {  // Verificar si el comando contiene el substring
            printf("%d. %s\n", favs[i].id, favs[i].command);  // Mostrar el número y el comando
            found = 1;
        }
    }

    if (!found) {
        printf("No se encontraron comandos que contengan '%s'.\n", substring);
    }
}

void delete_favorite_by_number(int num) {
    int found = 0;

    // Buscar el comando por su número
    for (int i = 0; i < fav_count; i++) {
        if (favs[i].id == num) {
            found = 1;
            // Desplazar todos los comandos después del eliminado
            for (int j = i; j < fav_count - 1; j++) {
                favs[j] = favs[j + 1];
                favs[j].id = j + 1; // Ajustar el ID después de eliminar
            }
            fav_count--; // Reducir el número de favoritos
            break;
        }
    }

    if (found) {
        printf("Comando con ID %d eliminado.\n", num);
    } else {
        printf("No se encontró un comando con ID %d.\n", num);
    }
}

void delete_favorites(const char *numbers) {
    char *nums_copy = strdup(numbers);  // Hacer una copia de la cadena de números
    char *token = strtok(nums_copy, ",");
    
    // Eliminar cada número proporcionado
    while (token != NULL) {
        int num = atoi(token);  // Convertir el número a entero
        delete_favorite_by_number(num);  // Llamar a la función que elimina por número
        token = strtok(NULL, ",");
    }

    free(nums_copy);  // Liberar la memoria asignada
}

void add_to_favorites_and_file(const char *command, const char *fav_file_path) {
    if (fav_count < MAX_FAVS) {
        favs[fav_count].id = fav_count + 1;
        strncpy(favs[fav_count].command, command, MAX_LINE);
        fav_count++;

        // Escribir el comando en el archivo
        FILE *file = fopen(fav_file_path, "a");  // Abrir en modo de anexado
        if (file == NULL) {
            perror("Error al abrir el archivo de favoritos para escribir");
            return;
        }
        fprintf(file, "%d %s\n", favs[fav_count - 1].id, command);  // Escribir el comando con su ID
        fclose(file);

        printf("Comando agregado a favoritos y al archivo: %s\n", command);
    } else {
        printf("La lista de favoritos está llena\n");
    }
}



void create_favs_file(const char *path) {
    FILE *file = fopen(path, "w");  // Crear un archivo en modo escritura
    if (file == NULL) {
        perror("Error al crear el archivo de favoritos");
        return;
    }

    // Guardar la ruta del archivo en una variable global
    strncpy(fav_file, path, MAX_PATH);
    fav_file_created = 1;  // Marcar que se ha creado el archivo
    fclose(file);

    printf("Archivo de favoritos creado en: %s\n", path);
}

void set_recordatorio(int segundos, char *mensaje) {
    pid_t pid = fork(); // Crear un proceso hijo

    if (pid == 0) { // Código del proceso hijo
        sleep(segundos); // Espera la cantidad de segundos especificados
        printf("\n[Recordatorio]: %s\n", mensaje); // Mostrar el mensaje del recordatorio
        exit(0); // Termina el proceso hijo
    }
}

// FunciÃ³n para dividir el input en comandos y argumentos
void parse_input(char *input, char **args) {
    int i = 0;
    while (i < MAX_ARGS - 1 && (args[i] = strsep(&input, " \t")) != NULL) {
        if (*args[i] != '\0') {
            i++;
        }
    }
    args[i] = NULL;
}
// Esta funciÃ³n dividirÃ¡ la entrada del usuario por pipes (|) y devolverÃ¡ un array de comandos
int parse_pipes(char *input, char **commands) {
    int i = 0;
    char *token = strtok(input, "|");
    while (token != NULL && i < MAX_COMMANDS) {
        commands[i++] = token;
        token = strtok(NULL, "|");
    }
    commands[i] = NULL; // AsegÃºrate de que el array termine con NULL
    return i; // Devuelve el nÃºmero de comandos
}

void execute_multiple_pipes(char ***commands, int num_commands) {
    int pipefd[2 * (num_commands - 1)]; // Crear pipes suficientes para todas las conexiones
    pid_t pids[num_commands]; // Almacenar los PIDs de los procesos

    // Crear los pipes
    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipefd + 2 * i) == -1) {
            perror("pipe failed");
            exit(1);
        }
    }

    // Ejecutar cada comando en su propio proceso
    for (int i = 0; i < num_commands; i++) {
        pids[i] = fork();
        if (pids[i] == -1) {
            perror("fork failed");
            exit(1);
        }

        if (pids[i] == 0) { // CÃ³digo del hijo
            // Redirigir la entrada y salida segÃºn sea necesario
            if (i > 0) {
                dup2(pipefd[(i - 1) * 2], STDIN_FILENO); // Entrada del pipe anterior
            }
            if (i < num_commands - 1) {
                dup2(pipefd[i * 2 + 1], STDOUT_FILENO); // Salida al prÃ³ximo pipe
            }

            // Cerrar todos los pipes en el proceso hijo
            for (int j = 0; j < 2 * (num_commands - 1); j++) {
                close(pipefd[j]);
            }

            // Ejecutar el comando
            if (execvp(commands[i][0], commands[i]) < 0) {
                perror("execvp failed");
                exit(1);
            }
        }
    }

    // Cerrar todos los pipes en el proceso padre
    for (int i = 0; i < 2 * (num_commands - 1); i++) {
        close(pipefd[i]);
    }

    // Esperar a que terminen todos los hijos
    for (int i = 0; i < num_commands; i++) {
        waitpid(pids[i], NULL, 0);
    }
}


int main() {
    char input[1024];           // Buffer para almacenar la entrada del usuario
    char *commands[MAX_COMMANDS]; // Array de comandos separados por pipes
    char *args[MAX_ARGS];        // Array de argumentos para cada comando
    int num_commands;
    char cwd[MAX_PATH];  // Para almacenar el directorio de trabajo actual


    while (1) {
        // Obtener el directorio de trabajo actual y mostrarlo en el prompt verde
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("\033[1;32m%s> \033[0m", cwd);  // Texto verde
        } else {
            perror("getcwd failed");
        }

        fflush(stdout);

        // Leer la entrada del usuario
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break; // Salir si se recibe EOF
        }

        // Eliminar el salto de lÃ­nea al final de la entrada
        input[strcspn(input, "\n")] = 0;

        // Si la entrada es solo un Enter (cadena vacÃ­a), saltar a la siguiente iteraciÃ³n
        if (strlen(input) == 0) {
            continue;
        }
        if (strncmp(input, "favs crear", 10) == 0) {
            char *path = input + 11;  // Extraer la ruta del comando
            create_favs_file(path);
            continue;  // Saltar al siguiente ciclo del bucle
        }

        if (strncmp(input, "favs", 4) != 0) {  // Excluir los comandos que empiezan con "favs"
            if (!is_favorite(input)) {  // Verificar si el comando ya está en favoritos
                add_to_favorites_and_file(input, "misfavoritos.txt");
            }
        }

        if (strcmp(input, "favs mostrar") == 0) {
            show_favorites();  // Llamar a la función para mostrar los favoritos
            continue;  // Volver al prompt después de mostrar la lista
        }

        if (strncmp(input, "favs eliminar", 13) == 0) {
            // Extraer los números después de "favs eliminar "
            char *numbers = input + 14;  // Obtener la parte con los números
            delete_favorites(numbers);   // Llamar a la función que elimina los comandos
            continue;  // Volver al prompt después de eliminar
        }

        if (strncmp(input, "favs buscar", 11) == 0) {
            // Extraer el substring después de "favs buscar "
            char *substring = input + 12;  // Obtener la parte con la subcadena
            search_favorites(substring);   // Llamar a la función para buscar favoritos
            continue;  // Volver al prompt después de la búsqueda
        }

        if (strcmp(input, "favs borrar") == 0) {
            clear_favorites();  // Llamar a la función para borrar todos los favoritos
            continue;  // Volver al prompt después de borrar los favoritos
        }


           // Verificar si el usuario ingresÃ³ "exit" para salir del programa
        if (strcmp(input, "exit") == 0) {
            printf("Goodnigh babylonia...\n");
            // Eliminar el archivo de favoritos antes de salir
            if (remove("misfavoritos.txt") == 0) {
                printf("El archivo misfavoritos.txt ha sido eliminado.\n");
            } else {
                perror("Error al eliminar el archivo misfavoritos.txt");
            }
            // Crear un nuevo archivo con el mismo nombre
            FILE *new_file = fopen("misfavoritos.txt", "w");
            if (new_file == NULL) {
                perror("Error al crear un nuevo archivo misfavoritos.txt");
            } else {
                printf("Se ha creado un nuevo archivo misfavoritos.txt para la siguiente ejecución\n");
                fclose(new_file); // Cerrar el archivo después de crearlo
            }
            
            exit(0); // Terminar el programa
        }
        // Verificar si el comando es "set recordatorio"
        if (strncmp(input, "set recordatorio", 16) == 0) {
            char *token = strtok(input, " "); // Separar el comando
            token = strtok(NULL, " "); // Obtener el siguiente token ("recordatorio")
            token = strtok(NULL, " "); // Obtener el tiempo en segundos
            int segundos = atoi(token); // Convertir a entero
            token = strtok(NULL, "\""); // Obtener el mensaje entre comillas

            // Llamar a la función para crear el recordatorio
            set_recordatorio(segundos, token);
            continue; // Volver al inicio del ciclo
        }


        // Verificar y manejar el comando cd
        if (strncmp(input, "cd", 2) == 0) {
            char *path = input + 3;  // Extraer la ruta del comando cd
            if (chdir(path) != 0) {
               perror("cd failed");  // Manejar errores si chdir() falla
            }
           continue;  // Volver al prompt después de cambiar el directorio
        }

        // Dividir la entrada en comandos separados por pipes
        num_commands = parse_pipes(input, commands);

        // Para cada comando, convertirlo en un array de argumentos
        char **cmd_args[num_commands];  // Array de arrays para los argumentos de cada comando
        for (int i = 0; i < num_commands; i++) {
            cmd_args[i] = malloc(MAX_ARGS * sizeof(char *)); // Asignar memoria para los argumentos
            parse_input(commands[i], cmd_args[i]); // Llenar cmd_args con los argumentos
        }

        // Ejecutar los comandos conectados por pipes
        execute_multiple_pipes(cmd_args, num_commands);

        // Liberar la memoria asignada
        for (int i = 0; i < num_commands; i++) {
            free(cmd_args[i]);
        }

            }

            return 0;
        }
