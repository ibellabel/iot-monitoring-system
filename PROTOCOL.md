# PROTOCOL.md

Especificacion del protocolo de aplicacion para el IoT Monitoring System.

## 1. Resumen

- **Transporte**: TCP
- **Puerto por defecto**: 9000
- **Codificacion**: texto plano (UTF-8 recomendado)
- **Delimitador de campos**: `|`
- **Terminador de mensaje**: salto de linea `\n`
- **Patron**: request/response con eventos asincronos enviados por servidor

Cada mensaje debe enviarse en una sola linea y finalizar con `\n`.

## 2. Formato general

```text
COMANDO|arg1|arg2|...\n
```

Reglas:

- `COMANDO` en mayusculas.
- Campos vacios permitidos solo donde aplique (ejemplo: `STATUS|\n`).
- Los valores no deben contener `|` ni saltos de linea.

## 3. Comandos cliente -> servidor

## 3.1 AUTH

Autentica usuario y establece rol para la sesion.

**Request**

```text
AUTH|usuario|contrasena\n
```

**Response OK**

```text
RESPONSE|OK|role=<rol>\n
```

**Response ERROR**

```text
RESPONSE|ERROR|credenciales invalidas\n
```

Notas:

- Roles esperados actuales: `operator`, `sensor`.

## 3.2 REGISTER

Registra o reactiva un sensor.

**Request**

```text
REGISTER|sensor|tipo|id\n
```

**Response OK**

```text
RESPONSE|OK|sensor registrado\n
```

**Response ERROR**

```text
RESPONSE|ERROR|max sensores alcanzado\n
```

Notas:

- `tipo` esperado: `temperatura`, `vibracion`, `energia`, `humedad`, etc.
- `id` debe ser unico por sensor.

## 3.3 MEASURE

Reporta una medicion de un sensor.

**Request**

```text
MEASURE|sensor_id|valor\n
```

**Response OK**

```text
RESPONSE|OK|medicion recibida\n
```

## 3.4 STATUS

Solicita snapshot del estado de sensores.

**Request**

```text
STATUS|\n
```

**Response** (multilinea)

Primera linea:

```text
RESPONSE|OK|sensores_activos=N\n
```

Luego cero o mas lineas:

```text
SENSOR|id|tipo|valor|estado\n
```

Donde `estado` tipicamente es `activo` o `inactivo`.

## 4. Mensajes servidor -> cliente (proactivos)

El servidor puede enviar estos mensajes sin haber request inmediato:

## 4.1 ALERT

```text
ALERT|id|nivel|desc\n
```

Ejemplo:

```text
ALERT|sensor_001|ALTO|valor=88.50 supera umbral\n
```

## 4.2 EVENT

```text
EVENT|tipo|p1|p2\n
```

Ejemplos comunes:

```text
EVENT|SENSOR_CONNECTED|sensor_001|temperatura\n
EVENT|CLIENT_DISCONNECTED|sensor_001\n
```

## 4.3 MEASURE (broadcast)

```text
MEASURE|id|valor\n
```

Uso: actualizaciones en vivo para clientes operador.

## 5. Flujo de comunicacion recomendado

## 5.1 Sensor

1. Conectar TCP a `<host>:9000`.
2. Enviar `AUTH|sensor1|sensor\n`.
3. Enviar `REGISTER|sensor|<tipo>|<id>\n`.
4. En bucle: enviar `MEASURE|<id>|<valor>\n` cada intervalo fijo.
5. Si se desconecta, reconectar y repetir desde AUTH.

## 5.2 Operador

1. Conectar TCP a `<host>:9000`.
2. Enviar `AUTH|admin|1234\n`.
3. Solicitar `STATUS|\n` para estado inicial.
4. Mantener lectura continua para procesar `MEASURE`, `EVENT`, `ALERT`.
5. Solicitar `STATUS|\n` manualmente cuando se requiera refresco completo.

## 6. Manejo de errores

Errores de aplicacion usan formato:

```text
RESPONSE|ERROR|<descripcion>\n
```

Errores conocidos actuales:

- `credenciales invalidas`
- `max sensores alcanzado`
- `comando desconocido`

Recomendaciones para clientes:

- Si reciben `ERROR` en AUTH: cerrar sesion y solicitar credenciales validas.
- Si hay timeout o socket cerrado: reconexion con backoff (ejemplo 5 segundos).
- Validar que cada mensaje termine con `\n` antes de parsear.

## 7. Ejemplos de sesion

```text
# Sensor
C -> S: AUTH|sensor1|sensor\n
S -> C: RESPONSE|OK|role=sensor\n
C -> S: REGISTER|sensor|temperatura|sensor_001\n
S -> C: RESPONSE|OK|sensor registrado\n
C -> S: MEASURE|sensor_001|82.75\n
S -> C: RESPONSE|OK|medicion recibida\n
S -> Operadores: MEASURE|sensor_001|82.75\n
S -> Operadores: ALERT|sensor_001|ALTO|valor=82.75 supera umbral\n
```

```text
# Operador
C -> S: AUTH|admin|1234\n
S -> C: RESPONSE|OK|role=operator\n
C -> S: STATUS|\n
S -> C: RESPONSE|OK|sensores_activos=2\n
S -> C: SENSOR|sensor_001|temperatura|82.75|activo\n
S -> C: SENSOR|sensor_002|vibracion|2.10|activo\n
```

## 8. Consideraciones y limitaciones

- No hay cifrado TLS en esta version del protocolo.
- No hay versionado explicito de protocolo (se recomienda agregar `HELLO|version` en futuras iteraciones).
- La autenticacion actual es simple y hardcodeada para entorno academico.
- El parsing del servidor es tolerante pero basico; mensajes malformados pueden derivar en `comando desconocido`.
