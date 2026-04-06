# IoT Monitoring System

Proyecto de sistemas distribuidos IoT con arquitectura cliente-servidor.

- Servidor en C con sockets Berkeley y manejo multicliente con hilos.
- Clientes en Python:
  - `clientes/sensor.py`: simula 5 sensores (temperatura, vibracion, energia, humedad).
  - `clientes/operador.py`: interfaz grafica para monitoreo y alertas.
- Interfaz HTTP de estado en puerto 8080.
- Contenerizacion del servidor con Docker.

## Estado del proyecto

El sistema esta funcional para demostracion y pruebas locales/remotas. Quedaron pendientes dos entregables:

1. Resolucion DNS automatica dentro del cliente.
   - Actualmente el host por defecto esta hardcodeado como `localhost`.
   - Como alternativa temporal, ambos clientes aceptan host/puerto por linea de comandos.
2. Cliente en segundo lenguaje.
   - En este repositorio solo se incluye cliente de sensores y cliente operador en Python.

## Arquitectura

### Componentes

- **Servidor IoT (`server/server.c`)**
  - Escucha conexiones TCP en el puerto configurable (normalmente 9000).
  - Atiende multiples clientes con un hilo por conexion.
  - Procesa comandos del protocolo de texto.
  - Mantiene estado de sensores en memoria.
  - Notifica eventos/alertas de forma proactiva a operadores conectados.
  - Genera logs en archivo.

- **Servidor HTTP embebido**
  - Corre dentro del mismo proceso en un hilo separado.
  - Expone una vista HTML del estado en `http://<host>:8080`.

- **Cliente sensor (`clientes/sensor.py`)**
  - Simula 5 sensores concurrentes.
  - Flujo: `AUTH -> REGISTER -> MEASURE` periodico.
  - Reconexion automatica si el servidor cae.

- **Cliente operador (`clientes/operador.py`)**
  - GUI en tkinter.
  - Flujo: `AUTH -> STATUS` y recepcion en vivo de `MEASURE`, `EVENT`, `ALERT`.

## Requisitos

### Sistema

- Linux/Mac/WSL o Windows con entorno compatible para GCC y Python.
- Puertos disponibles:
  - 9000/TCP (protocolo IoT)
  - 8080/TCP (HTTP)

### Dependencias

- GCC (compilador C)
- pthread (normalmente incluida en Linux)
- Python 3.9+
- tkinter (para `operador.py`; en Linux puede requerir instalacion adicional)
- Docker (opcional para ejecutar el servidor en contenedor)

## Estructura del repositorio

```text
clientes/
  operador.py
  sensor.py
server/
  Dockerfile
  logs.txt
  server.c
```

## Compilar y ejecutar servidor (local)

Desde la carpeta `server`:

```bash
gcc -o server server.c -lpthread
./server 9000 logs.txt
```

Salida esperada (aprox.):

- `[SERVER] Servidor IoT escuchando en puerto 9000`
- `[SERVER] Interfaz web en puerto 8080`

## Ejecutar clientes

> Todos los comandos pueden ejecutarse desde la raiz del repo o desde la carpeta `clientes`.

### 1) Cliente de sensores (simulacion)

```bash
python clientes/sensor.py
```

Con host/puerto remotos:

```bash
python clientes/sensor.py <HOST> <PUERTO>
```

Ejemplo:

```bash
python clientes/sensor.py 54.123.45.67 9000
```

### 2) Cliente operador (GUI)

```bash
python clientes/operador.py
```

Con host/puerto remotos:

```bash
python clientes/operador.py <HOST> <PUERTO>
```

Ejemplo:

```bash
python clientes/operador.py ec2-54-123-45-67.compute-1.amazonaws.com 9000
```

## Monitoreo web

Con el servidor activo, abrir:

```text
http://localhost:8080
```

Remoto (AWS):

```text
http://<DNS_O_IP_PUBLICA>:8080
```

## Docker

### Construir imagen

Opcion A (desde `server`):

```bash
docker build -t iot-server:latest .
```

Opcion B (desde raiz):

```bash
docker build -t iot-server:latest -f server/Dockerfile server
```

### Ejecutar contenedor

```bash
docker run -d --name iot-server \
  -p 9000:9000 \
  -p 8080:8080 \
  -v iot_logs:/app/logs \
  iot-server:latest
```

### Ver logs

```bash
docker logs -f iot-server
```

### Detener y eliminar

```bash
docker stop iot-server
docker rm iot-server
```

## Protocolo

La especificacion completa esta en `PROTOCOL.md`.

## Despliegue en AWS

La guia de despliegue (EC2 + Docker + Route 53) esta en `DEPLOY.md`.

## Limitaciones conocidas

- Host por defecto hardcodeado en clientes (`localhost`); no hay resolucion DNS automatica implementada en codigo.
- Sin TLS en el canal TCP del protocolo.
- Autenticacion simple hardcodeada para demostracion.
- Estado en memoria del servidor (sin persistencia estructurada de sensores).

## Credenciales de prueba

- Operador: `admin` / `1234`
- Sensor: `sensor1` / `sensor`

## Licencia

Uso academico para proyecto universitario.
