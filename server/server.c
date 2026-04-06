#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <signal.h>

#define MAX_CLIENTS 50
#define BUFFER_SIZE 1024
#define MAX_SENSORS 50
#define MAX_NAME 64

// ─── Estructuras ───────────────────────────────────────────────
typedef struct {
    char id[MAX_NAME];
    char type[MAX_NAME];       // temperatura, vibracion, energia
    char last_value[MAX_NAME];
    int  active;
    int  socket_fd;
} Sensor;

typedef struct {
    int  socket_fd;
    char role[MAX_NAME];       // sensor u operator
    char username[MAX_NAME];
    struct sockaddr_in addr;
} Client;

// ─── Globales ──────────────────────────────────────────────────
Sensor   sensors[MAX_SENSORS];
int      sensor_count = 0;
Client   operators[MAX_CLIENTS];
int      operator_count = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
FILE    *log_file = NULL;

// ─── Logging ───────────────────────────────────────────────────
void log_event(const char *client_ip, int client_port,
               const char *msg, const char *response) {
    time_t now = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));

    char line[512];
    snprintf(line, sizeof(line),
             "[%s] IP=%s PORT=%d MSG=\"%s\" RESP=\"%s\"\n",
             ts, client_ip, client_port, msg, response);

    printf("%s", line);
    if (log_file) { fprintf(log_file, "%s", line); fflush(log_file); }
}

// ─── Autenticación (servicio externo simulado por DNS lookup) ──
// En producción aquí harías una petición HTTP al servicio externo.
// Para este proyecto devolvemos rol según usuario conocido.
int authenticate(const char *user, const char *pass, char *role_out) {
    // Usuarios hardcodeados SOLO en este módulo de auth (simulando servicio externo)
    if (strcmp(user, "admin") == 0 && strcmp(pass, "1234") == 0) {
        strcpy(role_out, "operator"); return 1;
    }
    if (strcmp(user, "sensor1") == 0 && strcmp(pass, "sensor") == 0) {
        strcpy(role_out, "sensor"); return 1;
    }
    return 0;
}

// ─── Notificar a todos los operadores ─────────────────────────
void notify_operators(const char *msg) {
    pthread_mutex_lock(&lock);
    for (int i = 0; i < operator_count; i++) {
        if (operators[i].socket_fd > 0) {
            send(operators[i].socket_fd, msg, strlen(msg), 0);
        }
    }
    pthread_mutex_unlock(&lock);
}

// ─── Procesar mensaje del cliente ──────────────────────────────
void process_message(int client_fd, const char *msg,
                     const char *client_ip, int client_port,
                     char *role, char *client_id) {
    char response[BUFFER_SIZE] = {0};
    char cmd[64], a1[128], a2[128], a3[128];
    memset(cmd,0,sizeof(cmd)); memset(a1,0,sizeof(a1));
    memset(a2,0,sizeof(a2)); memset(a3,0,sizeof(a3));

    // Parsear: CMD|arg1|arg2|arg3
    sscanf(msg, "%63[^|]|%127[^|]|%127[^|]|%127s", cmd, a1, a2, a3);

    // AUTH|usuario|contraseña
    if (strcmp(cmd, "AUTH") == 0) {
        char r[32] = {0};
        if (authenticate(a1, a2, r)) {
            strcpy(role, r);
            strcpy(client_id, a1);
            snprintf(response, sizeof(response), "RESPONSE|OK|role=%s\n", r);
        } else {
            snprintf(response, sizeof(response), "RESPONSE|ERROR|credenciales invalidas\n");
        }
        send(client_fd, response, strlen(response), 0);
        log_event(client_ip, client_port, msg, response);
        return;
    }

    // REGISTER|sensor|tipo|id
    if (strcmp(cmd, "REGISTER") == 0 && strcmp(a1, "sensor") == 0) {
        pthread_mutex_lock(&lock);
        int found = -1;
        for (int i = 0; i < sensor_count; i++)
            if (strcmp(sensors[i].id, a3) == 0) { found = i; break; }
        if (found == -1 && sensor_count < MAX_SENSORS) {
            found = sensor_count++;
            strncpy(sensors[found].id,   a3, MAX_NAME-1);
            strncpy(sensors[found].type, a2, MAX_NAME-1);
            strcpy(sensors[found].last_value, "N/A");
        }
        if (found != -1) {
            sensors[found].active    = 1;
            sensors[found].socket_fd = client_fd;
            strcpy(role, "sensor");
            strcpy(client_id, a3);
            snprintf(response, sizeof(response), "RESPONSE|OK|sensor registrado\n");
        } else {
            snprintf(response, sizeof(response), "RESPONSE|ERROR|max sensores alcanzado\n");
        }
        pthread_mutex_unlock(&lock);
        send(client_fd, response, strlen(response), 0);
        log_event(client_ip, client_port, msg, response);

        // Notificar operadores
        char notif[256];
        snprintf(notif, sizeof(notif), "EVENT|SENSOR_CONNECTED|%s|%s\n", a3, a2);
        notify_operators(notif);
        return;
    }

    // MEASURE|sensor_id|valor
    if (strcmp(cmd, "MEASURE") == 0) {
        pthread_mutex_lock(&lock);
        for (int i = 0; i < sensor_count; i++) {
            if (strcmp(sensors[i].id, a1) == 0) {
                strncpy(sensors[i].last_value, a2, MAX_NAME-1);
                break;
            }
        }
        pthread_mutex_unlock(&lock);
        snprintf(response, sizeof(response), "RESPONSE|OK|medicion recibida\n");
        send(client_fd, response, strlen(response), 0);
        log_event(client_ip, client_port, msg, response);

        // Detectar anomalia simple (temperatura > 80)
        double val = atof(a2);
        char notif[256];
        snprintf(notif, sizeof(notif), "MEASURE|%s|%s\n", a1, a2);
        notify_operators(notif);
        if (val > 80.0) {
            snprintf(notif, sizeof(notif),
                     "ALERT|%s|ALTO|valor=%s supera umbral\n", a1, a2);
            notify_operators(notif);
        }
        return;
    }

    // STATUS|
    if (strcmp(cmd, "STATUS") == 0) {
        char status[BUFFER_SIZE] = {0};
        snprintf(status, sizeof(status), "RESPONSE|OK|sensores_activos=%d\n", sensor_count);
        pthread_mutex_lock(&lock);
        for (int i = 0; i < sensor_count; i++) {
            char line[128];
            snprintf(line, sizeof(line), "SENSOR|%s|%s|%s|%s\n",
                     sensors[i].id, sensors[i].type,
                     sensors[i].last_value,
                     sensors[i].active ? "activo" : "inactivo");
            strncat(status, line, BUFFER_SIZE - strlen(status) - 1);
        }
        pthread_mutex_unlock(&lock);
        send(client_fd, status, strlen(status), 0);
        log_event(client_ip, client_port, msg, "STATUS enviado");
        return;
    }

    // Comando desconocido
    snprintf(response, sizeof(response), "RESPONSE|ERROR|comando desconocido\n");
    send(client_fd, response, strlen(response), 0);
    log_event(client_ip, client_port, msg, response);
}

// ─── Hilo por cliente ──────────────────────────────────────────
typedef struct { int fd; struct sockaddr_in addr; } ThreadArg;

void *client_thread(void *arg) {
    ThreadArg *ta = (ThreadArg *)arg;
    int fd = ta->fd;
    struct sockaddr_in addr = ta->addr;
    free(ta);

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, client_ip, sizeof(client_ip));
    int  client_port = ntohs(addr.sin_port);

    char role[32]      = {0};
    char client_id[64] = {0};
    char buf[BUFFER_SIZE];

    // Registrar como operador hasta saber su rol
    pthread_mutex_lock(&lock);
    if (operator_count < MAX_CLIENTS) {
        operators[operator_count].socket_fd = fd;
        operators[operator_count].addr      = addr;
        operator_count++;
    }
    pthread_mutex_unlock(&lock);

    while (1) {
        memset(buf, 0, sizeof(buf));
        int n = recv(fd, buf, sizeof(buf)-1, 0);
        if (n <= 0) break;

        // Puede llegar más de un mensaje junto; procesamos línea a línea
        char *line = strtok(buf, "\n");
        while (line) {
            if (strlen(line) > 0)
                process_message(fd, line, client_ip, client_port,
                                role, client_id);
            line = strtok(NULL, "\n");
        }
    }

    // Limpiar al desconectarse
    pthread_mutex_lock(&lock);
    for (int i = 0; i < sensor_count; i++)
        if (sensors[i].socket_fd == fd) sensors[i].active = 0;
    for (int i = 0; i < operator_count; i++)
        if (operators[i].socket_fd == fd) operators[i].socket_fd = -1;
    pthread_mutex_unlock(&lock);

    char notif[128];
    snprintf(notif, sizeof(notif), "EVENT|CLIENT_DISCONNECTED|%s\n", client_id);
    notify_operators(notif);

    log_event(client_ip, client_port, "DISCONNECT", "conexion cerrada");
    close(fd);
    return NULL;
}

// ─── HTTP básico ───────────────────────────────────────────────
void *http_thread(void *arg) {
    int port = 8080;
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);
    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);
    printf("[HTTP] Servidor web en puerto %d\n", port);

    while (1) {
        int client = accept(server_fd, NULL, NULL);
        if (client < 0) continue;
        char req[2048] = {0};
        recv(client, req, sizeof(req)-1, 0);

        // Construir HTML con estado actual
        char body[4096] = {0};
        snprintf(body, sizeof(body),
            "<!DOCTYPE html><html><head>"
            "<meta charset='utf-8'>"
            "<title>IoT Monitor</title>"
            "<style>body{font-family:sans-serif;background:#1a1a2e;color:#eee;padding:20px}"
            "h1{color:#00d4ff}table{border-collapse:collapse;width:100%%}"
            "th,td{border:1px solid #444;padding:8px;text-align:left}"
            "th{background:#16213e}</style>"
            "</head><body>"
            "<h1>🌐 IoT Monitoring System</h1>"
            "<h2>Sensores activos: %d</h2>"
            "<table><tr><th>ID</th><th>Tipo</th><th>Último valor</th><th>Estado</th></tr>",
            sensor_count);

        pthread_mutex_lock(&lock);
        for (int i = 0; i < sensor_count; i++) {
            char row[256];
            snprintf(row, sizeof(row),
                     "<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>",
                     sensors[i].id, sensors[i].type,
                     sensors[i].last_value,
                     sensors[i].active ? "🟢 Activo" : "🔴 Inactivo");
            strncat(body, row, sizeof(body)-strlen(body)-1);
        }
        pthread_mutex_unlock(&lock);
        strncat(body, "</table></body></html>", sizeof(body)-strlen(body)-1);

        char response[8192];
        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: text/html; charset=utf-8\r\n"
                 "Content-Length: %zu\r\n"
                 "Connection: close\r\n\r\n%s",
                 strlen(body), body);
        send(client, response, strlen(response), 0);
        close(client);
    }
    return NULL;
}

// ─── Main ──────────────────────────────────────────────────────
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <puerto> <archivoDeLogs>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);

    log_file = fopen(argv[2], "a");
    if (!log_file) { perror("fopen"); return 1; }

    // Ignorar SIGPIPE (cliente desconectado)
    signal(SIGPIPE, SIG_IGN);

    // Hilo HTTP
    pthread_t http_tid;
    pthread_create(&http_tid, NULL, http_thread, NULL);

    // Socket TCP principal
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }
    listen(server_fd, MAX_CLIENTS);
    printf("[SERVER] Servidor IoT escuchando en puerto %d\n", port);
    printf("[SERVER] Interfaz web en puerto 8080\n");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) continue;

        ThreadArg *ta = malloc(sizeof(ThreadArg));
        ta->fd   = client_fd;
        ta->addr = client_addr;

        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, ta);
        pthread_detach(tid);
    }

    fclose(log_file);
    return 0;
}