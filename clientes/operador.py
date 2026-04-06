import socket
import threading
import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import sys
from datetime import datetime

# ─── Configuración ─────────────────────────────────────────────
SERVER_HOST = "localhost"   # Cambiar por el dominio DNS de AWS
SERVER_PORT = 9000

class OperadorApp:
    def __init__(self, root):
        self.root = root
        self.root.title("🌐 IoT Monitoring System - Operador")
        self.root.geometry("900x600")
        self.root.configure(bg="#1a1a2e")

        self.socket = None
        self.connected = False
        self.sensores = {}  # id -> {tipo, valor, estado}

        self._build_ui()

    # ─── UI ────────────────────────────────────────────────────
    def _build_ui(self):
        # Colores
        BG      = "#1a1a2e"
        PANEL   = "#16213e"
        ACCENT  = "#00d4ff"
        FG      = "#eaeaea"
        RED     = "#ff4d4d"
        GREEN   = "#00ff99"

        # ── Barra de conexión ──
        top = tk.Frame(self.root, bg=PANEL, pady=8)
        top.pack(fill=tk.X)

        tk.Label(top, text="Servidor:", bg=PANEL, fg=FG).pack(side=tk.LEFT, padx=(10,2))
        self.host_var = tk.StringVar(value=SERVER_HOST)
        tk.Entry(top, textvariable=self.host_var, width=20, bg="#0f3460", fg=FG,
                 insertbackground=FG).pack(side=tk.LEFT, padx=2)

        tk.Label(top, text="Puerto:", bg=PANEL, fg=FG).pack(side=tk.LEFT, padx=(6,2))
        self.port_var = tk.StringVar(value=str(SERVER_PORT))
        tk.Entry(top, textvariable=self.port_var, width=6, bg="#0f3460", fg=FG,
                 insertbackground=FG).pack(side=tk.LEFT, padx=2)

        tk.Label(top, text="Usuario:", bg=PANEL, fg=FG).pack(side=tk.LEFT, padx=(6,2))
        self.user_var = tk.StringVar(value="admin")
        tk.Entry(top, textvariable=self.user_var, width=10, bg="#0f3460", fg=FG,
                 insertbackground=FG).pack(side=tk.LEFT, padx=2)

        tk.Label(top, text="Contraseña:", bg=PANEL, fg=FG).pack(side=tk.LEFT, padx=(6,2))
        self.pass_var = tk.StringVar(value="1234")
        tk.Entry(top, textvariable=self.pass_var, show="*", width=10, bg="#0f3460", fg=FG,
                 insertbackground=FG).pack(side=tk.LEFT, padx=2)

        self.btn_connect = tk.Button(top, text="Conectar", bg=ACCENT, fg="#000",
                                     font=("Arial", 9, "bold"),
                                     command=self.toggle_connect)
        self.btn_connect.pack(side=tk.LEFT, padx=10)

        self.status_label = tk.Label(top, text="● Desconectado", bg=PANEL, fg=RED,
                                     font=("Arial", 9, "bold"))
        self.status_label.pack(side=tk.LEFT)

        # ── Contenido principal ──
        main = tk.Frame(self.root, bg=BG)
        main.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

        # Panel izquierdo — sensores
        left = tk.Frame(main, bg=PANEL, padx=8, pady=8)
        left.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        tk.Label(left, text="📡 Sensores Activos", bg=PANEL, fg=ACCENT,
                 font=("Arial", 12, "bold")).pack(anchor=tk.W)

        cols = ("ID", "Tipo", "Último Valor", "Estado")
        self.tree = ttk.Treeview(left, columns=cols, show="headings", height=15)
        style = ttk.Style()
        style.theme_use("clam")
        style.configure("Treeview",
                         background=PANEL, foreground=FG,
                         rowheight=28, fieldbackground=PANEL)
        style.configure("Treeview.Heading",
                         background="#0f3460", foreground=ACCENT,
                         font=("Arial", 9, "bold"))
        style.map("Treeview", background=[("selected", "#0f3460")])

        for c in cols:
            self.tree.heading(c, text=c)
            self.tree.column(c, width=120)
        self.tree.pack(fill=tk.BOTH, expand=True, pady=6)

        btn_status = tk.Button(left, text="🔄 Actualizar Estado",
                               bg="#0f3460", fg=ACCENT,
                               command=self.request_status)
        btn_status.pack(pady=4)

        # Panel derecho — alertas y log
        right = tk.Frame(main, bg=PANEL, padx=8, pady=8, width=300)
        right.pack(side=tk.RIGHT, fill=tk.BOTH, padx=(10,0))
        right.pack_propagate(False)

        tk.Label(right, text="🚨 Alertas y Eventos", bg=PANEL, fg=ACCENT,
                 font=("Arial", 12, "bold")).pack(anchor=tk.W)

        self.alert_box = scrolledtext.ScrolledText(
            right, bg="#0d0d1a", fg=FG, height=20, width=35,
            font=("Courier", 9), state=tk.DISABLED)
        self.alert_box.pack(fill=tk.BOTH, expand=True, pady=6)
        self.alert_box.tag_config("alert", foreground="#ff4d4d")
        self.alert_box.tag_config("event", foreground="#00ff99")
        self.alert_box.tag_config("measure", foreground="#aaaaaa")

        btn_clear = tk.Button(right, text="🗑 Limpiar", bg="#0f3460", fg=FG,
                              command=self.clear_alerts)
        btn_clear.pack()

    # ─── Conexión ──────────────────────────────────────────────
    def toggle_connect(self):
        if not self.connected:
            self.connect()
        else:
            self.disconnect()

    def connect(self):
        host = self.host_var.get().strip()
        port = int(self.port_var.get().strip())
        user = self.user_var.get().strip()
        pwd  = self.pass_var.get().strip()
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.connect((host, port))
            self.connected = True

            # AUTH
            self.socket.sendall(f"AUTH|{user}|{pwd}\n".encode())
            resp = self.socket.recv(1024).decode().strip()
            if "ERROR" in resp:
                messagebox.showerror("Auth", f"Error: {resp}")
                self.socket.close(); self.connected = False
                return

            self.status_label.config(text="● Conectado", fg="#00ff99")
            self.btn_connect.config(text="Desconectar")
            self.log_alert(f"Conectado como {user}", "event")

            # Hilo receptor
            t = threading.Thread(target=self.receive_loop, daemon=True)
            t.start()

            # Pedir estado inicial
            self.request_status()

        except Exception as e:
            messagebox.showerror("Error", str(e))
            self.connected = False

    def disconnect(self):
        self.connected = False
        try: self.socket.close()
        except: pass
        self.status_label.config(text="● Desconectado", fg="#ff4d4d")
        self.btn_connect.config(text="Conectar")
        self.log_alert("Desconectado del servidor", "alert")

    # ─── Recepción de mensajes ─────────────────────────────────
    def receive_loop(self):
        buf = ""
        while self.connected:
            try:
                data = self.socket.recv(1024).decode()
                if not data:
                    break
                buf += data
                while "\n" in buf:
                    line, buf = buf.split("\n", 1)
                    if line:
                        self.root.after(0, self.process_message, line)
            except:
                break
        self.root.after(0, self.disconnect)

    def process_message(self, msg):
        parts = msg.split("|")
        cmd   = parts[0] if parts else ""

        if cmd == "RESPONSE" and len(parts) >= 3:
            # Parsear sensores del STATUS
            pass

        elif cmd == "SENSOR" and len(parts) >= 5:
            sid, tipo, valor, estado = parts[1], parts[2], parts[3], parts[4]
            self.sensores[sid] = {"tipo": tipo, "valor": valor, "estado": estado}
            self.update_table()

        elif cmd == "MEASURE" and len(parts) >= 3:
            sid, valor = parts[1], parts[2]
            if sid in self.sensores:
                self.sensores[sid]["valor"] = valor
                self.update_table()
            self.log_alert(f"📊 {sid}: {valor}", "measure")

        elif cmd == "ALERT" and len(parts) >= 4:
            self.log_alert(f"⚠️  ALERTA [{parts[1]}] {parts[3]}", "alert")

        elif cmd == "EVENT":
            self.log_alert(f"🔔 {' '.join(parts[1:])}", "event")

    def request_status(self):
        if self.connected:
            try:
                self.socket.sendall(b"STATUS|\n")
            except: pass

    # ─── UI helpers ────────────────────────────────────────────
    def update_table(self):
        for row in self.tree.get_children():
            self.tree.delete(row)
        for sid, info in self.sensores.items():
            self.tree.insert("", tk.END, values=(
                sid, info["tipo"], info["valor"], info["estado"]
            ))

    def log_alert(self, msg, tag="measure"):
        ts = datetime.now().strftime("%H:%M:%S")
        self.alert_box.config(state=tk.NORMAL)
        self.alert_box.insert(tk.END, f"[{ts}] {msg}\n", tag)
        self.alert_box.see(tk.END)
        self.alert_box.config(state=tk.DISABLED)

    def clear_alerts(self):
        self.alert_box.config(state=tk.NORMAL)
        self.alert_box.delete(1.0, tk.END)
        self.alert_box.config(state=tk.DISABLED)


# ─── Main ──────────────────────────────────────────────────────
if __name__ == "__main__":
    if len(sys.argv) >= 2: SERVER_HOST = sys.argv[1]
    if len(sys.argv) >= 3: SERVER_PORT = int(sys.argv[2])

    root = tk.Tk()
    app  = OperadorApp(root)
    root.mainloop()