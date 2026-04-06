import socket
import time
import random
import threading
import sys

# ─── Configuración ─────────────────────────────────────────────
SERVER_HOST = "localhost"   # Cambiar por el dominio DNS de AWS
SERVER_PORT = 9000

# Tipos de sensores simulados
SENSORES = [
    {"id": "sensor_001", "tipo": "temperatura",  "min": 15.0, "max": 95.0},
    {"id": "sensor_002", "tipo": "vibracion",    "min": 0.1,  "max": 10.0},
    {"id": "sensor_003", "tipo": "energia",      "min": 100,  "max": 500},
    {"id": "sensor_004", "tipo": "humedad",      "min": 20.0, "max": 90.0},
    {"id": "sensor_005", "tipo": "temperatura",  "min": 15.0, "max": 95.0},
]

def conectar_sensor(sensor):
    """Conecta un sensor al servidor y envía mediciones periódicas."""
    sid  = sensor["id"]
    tipo = sensor["tipo"]

    while True:
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((SERVER_HOST, SERVER_PORT))
            print(f"[{sid}] Conectado al servidor")

            # 1. Autenticarse
            s.sendall(f"AUTH|sensor1|sensor\n".encode())
            resp = s.recv(1024).decode().strip()
            print(f"[{sid}] AUTH → {resp}")

            # 2. Registrarse
            s.sendall(f"REGISTER|sensor|{tipo}|{sid}\n".encode())
            resp = s.recv(1024).decode().strip()
            print(f"[{sid}] REGISTER → {resp}")

            # 3. Enviar mediciones cada 3 segundos
            while True:
                valor = round(random.uniform(sensor["min"], sensor["max"]), 2)
                msg   = f"MEASURE|{sid}|{valor}\n"
                s.sendall(msg.encode())
                resp = s.recv(1024).decode().strip()
                print(f"[{sid}] MEASURE {valor} → {resp}")
                time.sleep(3)

        except (ConnectionRefusedError, BrokenPipeError, OSError) as e:
            print(f"[{sid}] Desconectado: {e}. Reintentando en 5s...")
            time.sleep(5)
        finally:
            try: s.close()
            except: pass

def main():
    global SERVER_HOST
    if len(sys.argv) >= 2:
        SERVER_HOST = sys.argv[1]
    if len(sys.argv) >= 3:
        SERVER_PORT_ARG = int(sys.argv[2])
    else:
        SERVER_PORT_ARG = SERVER_PORT

    print(f"Iniciando {len(SENSORES)} sensores → {SERVER_HOST}:{SERVER_PORT_ARG}")

    hilos = []
    for sensor in SENSORES:
        t = threading.Thread(target=conectar_sensor, args=(sensor,), daemon=True)
        t.start()
        hilos.append(t)
        time.sleep(0.5)  # pequeño delay entre conexiones

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nSensores detenidos.")

if __name__ == "__main__":
    main()