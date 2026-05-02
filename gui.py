import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import subprocess
import threading
import os
import csv

class RoutingGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("PDC Project: Multi-City OSM Routing Engine")
        self.root.geometry("1600x1000")
        
        # Presets
        self.monaco_nodes = [
            "21911883", "21911886", "21911894", "21911901", "21911908",
            "21911954", "21911969", "21912089", "21912093", "21912095",
            "21912097", "21912099", "21912962", "21912976", "21912993",
            "21913012", "21913117", "21913159", "21914339", "21914340",
            "21914341", "21914343", "21914359", "21914381", "21914573",
            "21914666", "21914722", "21914761", "21914797", "21914809"
        ]
        
        self.brussels_nodes = [
            "125892", "125893", "125896", "125898", "125904", "125907",
            "125914", "125915", "125916", "125942", "125943", "125944",
            "125945", "125946", "125947", "125949", "125953", "125955",
            "144921", "144922", "144924", "144925", "144934", "144935",
            "144936", "144937", "144938", "144939", "144941", "144942"
        ]

        # Light Theme
        self.bg_color = "#ffffff"
        self.sidebar_bg = "#f8fafc"
        self.accent_color = "#2563eb"
        self.text_color = "#0f172a"
        
        self.root.configure(bg=self.bg_color)
        self.setup_styles()
        self.create_widgets()
        
        self.process = None

    def setup_styles(self):
        style = ttk.Style()
        style.theme_use('clam')
        style.configure("TFrame", background=self.bg_color)
        style.configure("Sidebar.TFrame", background=self.sidebar_bg)
        style.configure("TLabel", background=self.bg_color, foreground=self.text_color, font=("Segoe UI", 10))
        style.configure("Sidebar.TLabel", background=self.sidebar_bg, foreground=self.text_color, font=("Segoe UI", 10))
        style.configure("Header.TLabel", font=("Segoe UI", 24, "bold"), foreground=self.accent_color, background=self.sidebar_bg)
        style.configure("SubHeader.TLabel", font=("Segoe UI", 12, "bold"), foreground="#475569", background=self.sidebar_bg)
        style.configure("TCombobox", fieldbackground="#ffffff", foreground="#000000")
        style.configure("TNotebook", background=self.bg_color, borderwidth=0)
        style.configure("TNotebook.Tab", background="#f1f5f9", foreground="#000000", padding=[20, 8], font=("Segoe UI", 10, "bold"))
        style.map("TNotebook.Tab", background=[("selected", self.accent_color)], foreground=[("selected", "#ffffff")])
        style.configure("TButton", font=("Segoe UI", 10, "bold"), padding=10)

    def create_widgets(self):
        # Sidebar
        sidebar = ttk.Frame(self.root, style="Sidebar.TFrame", width=420)
        sidebar.pack(side="left", fill="y")
        sidebar.pack_propagate(False)

        ttk.Label(sidebar, text="PDC OSM ROUTER", style="Header.TLabel").pack(pady=(40, 20), padx=40, anchor="w")

        cfg = ttk.Frame(sidebar, style="Sidebar.TFrame")
        cfg.pack(fill="both", expand=True, padx=40)

        # Map Selection
        ttk.Label(cfg, text="ACTIVE MAP SET", style="SubHeader.TLabel").pack(anchor="w", pady=(10, 5))
        self.map_var = tk.StringVar(value="monaco.osm")
        self.map_var.trace_add("write", self.on_map_change) # Update presets on change
        
        ttk.Radiobutton(cfg, text="Monaco (Micro-Scale)", variable=self.map_var, value="monaco.osm").pack(anchor="w")
        ttk.Radiobutton(cfg, text="Brussels (Urban-Scale)", variable=self.map_var, value="brussels.osm").pack(anchor="w")

        # Hyperparameters
        ttk.Label(cfg, text="HYPERPARAMETERS", style="SubHeader.TLabel").pack(anchor="w", pady=(20, 5))
        p_frame = ttk.Frame(cfg, style="Sidebar.TFrame")
        p_frame.pack(fill="x")
        ttk.Label(p_frame, text="Threads:", style="Sidebar.TLabel").grid(row=0, column=0, sticky="w", pady=2)
        self.threads_var = tk.IntVar(value=4)
        tk.Scale(p_frame, from_=1, to=16, orient="horizontal", variable=self.threads_var, bg=self.sidebar_bg, fg="#000000", highlightthickness=0).grid(row=0, column=1, sticky="ew", padx=10, pady=2)

        ttk.Label(p_frame, text="Delta (Δ):", style="Sidebar.TLabel").grid(row=1, column=0, sticky="w", pady=2)
        self.delta_var = tk.StringVar(value="150")
        ttk.Entry(p_frame, textvariable=self.delta_var, width=10).grid(row=1, column=1, sticky="w", padx=10, pady=2)

        ttk.Label(p_frame, text="Chunk/Landmarks:", style="Sidebar.TLabel").grid(row=2, column=0, sticky="w", pady=2)
        self.chunk_var = tk.StringVar(value="16")
        ttk.Entry(p_frame, textvariable=self.chunk_var, width=10).grid(row=2, column=1, sticky="w", padx=10, pady=2)

        # Task Tabs
        ttk.Label(cfg, text="ROUTING ENGINE TASKS", style="SubHeader.TLabel").pack(anchor="w", pady=(20, 5))
        self.task_tabs = ttk.Notebook(cfg)
        self.task_tabs.pack(fill="x", pady=5)

        # Single Query Tab
        sq_f = ttk.Frame(self.task_tabs, style="Sidebar.TFrame")
        self.task_tabs.add(sq_f, text="Single Query")
        
        ttk.Label(sq_f, text="Method:", style="Sidebar.TLabel").pack(anchor="w", pady=(10, 0))
        self.algo_var = tk.StringVar(value="dijkstra")
        ttk.Combobox(sq_f, textvariable=self.algo_var, values=["dijkstra", "bidirectional", "delta_stepping", "parallel_dijkstra", "landmark_astar"], state="readonly").pack(fill="x", pady=5)
        
        self.run_all_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(sq_f, text="Comparison Mode (Run All Methods)", variable=self.run_all_var).pack(anchor="w", pady=5)

        # Node Selection with Map-specific warnings
        self.preset_label = ttk.Label(sq_f, text="Presets for MONACO:", style="Sidebar.TLabel", foreground="#ef4444")
        self.preset_label.pack(anchor="w", pady=(10, 0))
        
        ttk.Label(sq_f, text="Source Node:", style="Sidebar.TLabel").pack(anchor="w")
        self.src_combo = ttk.Combobox(sq_f, values=self.monaco_nodes)
        self.src_combo.set(self.monaco_nodes[0])
        self.src_combo.pack(fill="x", pady=5)
        
        ttk.Label(sq_f, text="Target Node:", style="Sidebar.TLabel").pack(anchor="w")
        self.tgt_combo = ttk.Combobox(sq_f, values=self.monaco_nodes)
        self.tgt_combo.set(self.monaco_nodes[1])
        self.tgt_combo.pack(fill="x", pady=5)

        # Performance Tab
        bm_f = ttk.Frame(self.task_tabs, style="Sidebar.TFrame")
        self.task_tabs.add(bm_f, text="Performance")
        ttk.Label(bm_f, text="Experiment:", style="Sidebar.TLabel").pack(anchor="w", pady=(10, 0))
        self.bench_var = tk.StringVar(value="Algorithm Comparison")
        ttk.Combobox(bm_f, textvariable=self.bench_var, values=["Algorithm Comparison", "Thread Scaling Analysis", "Milestone 3 Running (N queries)"], state="readonly").pack(fill="x", pady=5)
        ttk.Label(bm_f, text="Query Count:", style="Sidebar.TLabel").pack(anchor="w")
        self.queries_var = tk.StringVar(value="100")
        ttk.Entry(bm_f, textvariable=self.queries_var).pack(fill="x", pady=5)

        # Action Buttons
        self.run_btn = ttk.Button(sidebar, text="START ENGINE", command=self.run_task)
        self.run_btn.pack(fill="x", padx=40, pady=(40, 10))
        self.stop_btn = ttk.Button(sidebar, text="STOP", command=self.stop_task, state="disabled")
        self.stop_btn.pack(fill="x", padx=40, pady=5)

        # Workspace
        main = ttk.Frame(self.root)
        main.pack(side="right", fill="both", expand=True, padx=30, pady=30)
        
        self.log_tab = ttk.Frame(main)
        self.log_tab.pack(fill="both", expand=True)
        ttk.Label(self.log_tab, text="SYSTEM TERMINAL", style="Header.TLabel").pack(pady=(0, 10), anchor="w")
        self.console = scrolledtext.ScrolledText(self.log_tab, bg="#ffffff", fg="#000000", font=("Consolas", 10), borderwidth=1, relief="solid")
        self.console.pack(fill="both", expand=True)

    def on_map_change(self, *args):
        m = self.map_var.get()
        if "monaco" in m:
            nodes = self.monaco_nodes
            self.preset_label.config(text="Presets for MONACO ONLY:", foreground="#ef4444")
        else:
            nodes = self.brussels_nodes
            self.preset_label.config(text="Presets for BRUSSELS ONLY:", foreground="#16a34a")
        
        self.src_combo.config(values=nodes)
        self.tgt_combo.config(values=nodes)
        self.src_combo.set(nodes[0])
        self.tgt_combo.set(nodes[1])

    def log(self, msg):
        self.console.insert(tk.END, f"{msg}\n")
        self.console.see(tk.END)

    def run_task(self):
        self.run_btn.config(state="disabled")
        self.stop_btn.config(state="normal")
        self.console.delete(1.0, tk.END)
        
        active_tab = self.task_tabs.index("current")
        if active_tab == 0 and self.run_all_var.get():
            threading.Thread(target=self.run_comparison_suite, daemon=True).start()
        else:
            threading.Thread(target=self.run_single_task, daemon=True).start()

    def run_single_task(self, algo_override=None):
        m_path = os.path.join("data", self.map_var.get())
        algo = algo_override if algo_override else self.algo_var.get()
        src = self.src_combo.get()
        tgt = self.tgt_combo.get()
        threads = str(self.threads_var.get())
        delta = self.delta_var.get()
        chunk = self.chunk_var.get()
        
        exe_ext = ".exe" if os.name == "nt" else ""
        
        if self.task_tabs.index("current") == 0:
            cmd = [os.path.join("build", f"gui_backend{exe_ext}"), m_path, algo, src, tgt, threads, delta, chunk]
        else:
            if "Comparison" in self.bench_var.get():
                cmd = [os.path.join("build", f"osm_parallel{exe_ext}"), m_path, threads, delta, self.queries_var.get()]
            else:
                cmd = [os.path.join("build", f"osm_m3{exe_ext}"), m_path, self.queries_var.get(), chunk]

        self.log(f"ENGINE> Running {algo} on {self.map_var.get()}...")
        try:
            self.process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, universal_newlines=True, encoding='utf-8', errors='replace')
            res_time = 0
            c_mode = False
            for line in self.process.stdout:
                clean_line = line.rstrip('\r\n')
                if "PATH_COORDS_START" in clean_line: 
                    c_mode = True
                    continue
                if "PATH_COORDS_END" in clean_line:
                    c_mode = False
                    continue
                if c_mode:
                    continue
                
                if "RESULT_TIME:" in clean_line:
                    try:
                        res_time = float(clean_line.split("RESULT_TIME:")[1].strip().split()[0])
                    except:
                        pass
                self.root.after(0, self.log, clean_line)
            self.process.wait()
            if algo_override: return res_time
            
            self.root.after(0, self.on_task_done)
        except Exception as e:
            self.log(f"ERROR: {e}")
            if not algo_override: self.root.after(0, self.on_task_done)

    def run_comparison_suite(self):
        algos = ["dijkstra", "bidirectional", "delta_stepping", "parallel_dijkstra", "landmark_astar"]
        for a in algos:
            self.run_single_task(a)
        self.root.after(0, self.on_task_done)

    def on_task_done(self):
        self.run_btn.config(state="normal")
        self.stop_btn.config(state="disabled")
        self.log("-" * 60 + "\nTASK COMPLETE.")

    def stop_task(self):
        if self.process: self.process.terminate()

if __name__ == "__main__":
    root = tk.Tk()
    app = RoutingGUI(root)
    root.mainloop()
