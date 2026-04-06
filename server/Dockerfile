# Imagen base con GCC para compilar C
FROM gcc:latest

# Directorio de trabajo dentro del contenedor
WORKDIR /app

# Copiar el código fuente del servidor
COPY server.c .

# Compilar el servidor
RUN gcc -o server server.c -lpthread

# Exponer los puertos
# 9000 -> protocolo IoT (TCP)
# 8080 -> interfaz web HTTP
EXPOSE 9000
EXPOSE 8080

# Crear carpeta para logs
RUN mkdir -p /app/logs

# Comando de inicio
CMD ["./server", "9000", "/app/logs/server.log"]