# DEPLOY.md

Guia de despliegue del IoT Monitoring System en AWS usando EC2 + Docker + Route 53.

## 1. Objetivo

Desplegar el servidor en una instancia EC2 para exponer:

- Puerto 9000/TCP (protocolo IoT)
- Puerto 8080/TCP (interfaz web)

Y asociar un dominio DNS administrado en Route 53.

## 2. Prerrequisitos

- Cuenta AWS con permisos sobre EC2, Security Groups y Route 53.
- Par de llaves SSH para acceso a EC2.
- Dominio registrado (en Route 53 o externo, delegando zona a Route 53).
- Docker instalado en la instancia EC2.

## 3. Crear instancia EC2

1. Ir a AWS Console -> EC2 -> Launch instance.
2. Elegir AMI (recomendado Ubuntu Server LTS).
3. Tipo de instancia sugerido para demo: `t2.micro` o `t3.micro`.
4. Seleccionar/crear key pair.
5. Configurar red y Security Group con reglas de entrada:
   - SSH: `22/TCP` desde tu IP.
   - IoT TCP: `9000/TCP` desde rango permitido (idealmente restringido).
   - HTTP monitor: `8080/TCP` desde rango permitido.
6. Lanzar instancia y esperar estado `running`.

## 4. Conectarse por SSH

```bash
ssh -i <tu-clave.pem> ubuntu@<IP_PUBLICA_EC2>
```

## 5. Instalar Docker en Ubuntu (si no esta instalado)

```bash
sudo apt update
sudo apt install -y docker.io
sudo systemctl enable docker
sudo systemctl start docker
sudo usermod -aG docker $USER
newgrp docker
```

Verificar:

```bash
docker --version
```

## 6. Subir codigo al servidor

Opciones:

- Clonar desde Git:

```bash
git clone <URL_DEL_REPO>
cd iot-monitoring-system
```

- O copiar por SCP desde local:

```bash
scp -i <tu-clave.pem> -r ./iot-monitoring-system ubuntu@<IP_PUBLICA_EC2>:~/
ssh -i <tu-clave.pem> ubuntu@<IP_PUBLICA_EC2>
cd ~/iot-monitoring-system
```

## 7. Construir imagen Docker

Desde la raiz del proyecto:

```bash
docker build -t iot-server:latest -f server/Dockerfile server
```

Alternativa si te ubicas en `server`:

```bash
cd server
docker build -t iot-server:latest .
```

## 8. Ejecutar contenedor

```bash
docker run -d --name iot-server \
  --restart unless-stopped \
  -p 9000:9000 \
  -p 8080:8080 \
  -v iot_logs:/app/logs \
  iot-server:latest
```

Verificar estado:

```bash
docker ps
docker logs -f iot-server
```

## 9. Validacion funcional

Desde tu maquina local:

- Verificar web:

```text
http://<IP_PUBLICA_EC2>:8080
```

- Ejecutar sensores:

```bash
python clientes/sensor.py <IP_PUBLICA_EC2> 9000
```

- Ejecutar operador:

```bash
python clientes/operador.py <IP_PUBLICA_EC2> 9000
```

## 10. Configurar Route 53

## 10.1 Crear/usar Hosted Zone

1. AWS Console -> Route 53 -> Hosted zones.
2. Crear zona publica para tu dominio (si no existe).
3. Si dominio externo, delegar NS hacia Route 53.

## 10.2 Crear registro DNS

1. Dentro de la hosted zone, crear registro `A`:
   - Name: por ejemplo `iot` (quedaria `iot.tudominio.com`).
   - Value: IP publica de EC2.
   - TTL: 300s (recomendado para pruebas).
2. Guardar y esperar propagacion.

## 10.3 Probar

```bash
nslookup iot.tudominio.com
```

Luego usar:

- Protocolo TCP: `iot.tudominio.com:9000`
- Web: `http://iot.tudominio.com:8080`

## 11. Nota importante sobre DNS en este proyecto

Pendiente funcional identificado:

- Los clientes tienen host por defecto hardcodeado en `localhost`.
- Para usar Route 53 hoy, debes pasar el host manualmente por argumento:

```bash
python clientes/sensor.py iot.tudominio.com 9000
python clientes/operador.py iot.tudominio.com 9000
```

Esto permite operar con DNS sin modificar el servidor, aunque no implementa descubrimiento DNS automatico en el cliente.

## 12. Operacion y mantenimiento basico

Comandos utiles:

```bash
docker restart iot-server
docker stop iot-server
docker rm iot-server
docker image ls
docker volume ls
```

Para actualizar version:

1. Traer cambios del repo.
2. Reconstruir imagen.
3. Re-crear contenedor.

## 13. Seguridad recomendada (minimo para entrega)

- Restringir `9000/TCP` y `8080/TCP` en Security Group a IPs de laboratorio cuando sea posible.
- Mantener `22/TCP` solo para IP administrativa.
- No usar credenciales hardcodeadas en produccion real.
- Considerar TLS y autenticacion robusta en futuras iteraciones.
