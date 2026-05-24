#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
MS72SF1 — Carte temps réel de suivi/comptage de personnes
==========================================================

Lit le flux série du radar mmWave MinewSemi MS72SF1 (60-64 GHz, plafond),
parse les pistes (position X/Y/Z + vitesse + ID), et les affiche sur une
carte 2D temps réel : position, ID, trajectoire et vecteur vitesse de
chaque cible, plus le compteur global.

Protocole (datasheet MS72SF1 §8.2), little-endian :
    HEADER   : 8 octets fixes 01 02 03 04 05 06 07 08
    LENGTH   : uint32  (longueur totale de la trame, header inclus)
    FRAME    : uint32  (numéro de trame)
    [ TLVs ] : suite de blocs { type:uint32, len:uint32, payload[len] }
        type == 1 : nuage de points (len = 0 sur ce firmware)
        type == 2 : pistes personnes -> N = len / 32, chaque piste = 8 floats :
                    Q, ID, X, Y, Z, Vx, Vy, Vz   (m et m/s)

Dépendances :
    sudo apt install python3-pyqt5 python3-pyqtgraph python3-serial python3-numpy
    # ou : pip install PyQt5 pyqtgraph pyserial numpy

Usage :
    python3 ms72sf1_radar_map.py                       # /dev/ttyUSB0 par défaut
    python3 ms72sf1_radar_map.py --port /dev/ttyUSB1
    python3 ms72sf1_radar_map.py --no-init --debug
"""

import sys
import time
import struct
import argparse
import threading
import queue
from collections import deque

import numpy as np
import serial
import pyqtgraph as pg
from pyqtgraph.Qt import QtCore, QtGui, QtWidgets

# ----------------------------------------------------------------------------
# Constantes protocole / réglages
# ----------------------------------------------------------------------------
HEADER = bytes((1, 2, 3, 4, 5, 6, 7, 8))
PERSON_SIZE = 32                 # 8 floats * 4 octets
TLV_POINTCLOUD = 1
TLV_TRACKS = 2
MIN_FRAME_LEN = 16               # header(8) + length(4) + frame(4)
MAX_FRAME_LEN = 8192             # garde-fou resync

# Endianness : la datasheet indique IEEE754 "small end" -> little-endian.
U32 = "<I"
F32 = "<f"

# Zone de détection par défaut (m). Correspond aux bornes AT par défaut ±300 cm.
DEFAULT_ZONE = dict(xmin=-3.0, xmax=3.0, ymin=-3.0, ymax=3.0)

# Commandes d'init AT (terminées par \n d'après la datasheet).
AT_INIT = [b"AT+DEBUG=3\n", b"AT+START\n"]   # mode 3 = trames binaires

# Affichage
TRAIL_LEN = 40                   # nb de points conservés par trajectoire
STALE_FRAMES = 8                 # piste oubliée après N trames sans la voir
VEL_SCALE = 0.5                  # longueur du vecteur vitesse (s -> m affichés)
MAX_PERSONS = 12                 # >10 théorique, marge

# --- Filtrage anti-faux-positifs (à calibrer face au matériel) ---
CONFIRM_FRAMES = 3       # une piste n'est comptée/affichée qu'après N trames vues
ZONE_SOFT = True         # rejeter en logiciel les cibles hors DEFAULT_ZONE
Z_MIN = None             # hauteur cible mini (m) : rejette les cibles basses (chiens) ; None = off
Z_MAX = None             # hauteur cible maxi (m) ; None = off

PALETTE = [
    (231, 76, 60), (52, 152, 219), (46, 204, 113), (241, 196, 15),
    (155, 89, 182), (26, 188, 156), (230, 126, 34), (52, 73, 94),
    (236, 64, 122), (149, 165, 166), (39, 174, 96), (211, 84, 0),
]


def color_for_id(track_id):
    return PALETTE[int(track_id) % len(PALETTE)]


# ----------------------------------------------------------------------------
# Thread de lecture série + parsing
# ----------------------------------------------------------------------------
class RadarReader(threading.Thread):
    """Lit le port série en continu, resynchronise sur le header, parse les
    trames et pousse la liste des cibles (frame_no, [persons]) dans une queue."""

    def __init__(self, port, baud, out_queue, send_init=True, debug=False):
        super().__init__(daemon=True)
        self.port = port
        self.baud = baud
        self.q = out_queue
        self.send_init = send_init
        self.debug = debug
        self._stop = threading.Event()
        self.status = "init"
        self.ser = None

    def stop(self):
        self._stop.set()

    # --- connexion / reconnexion ---------------------------------------
    def _open(self):
        while not self._stop.is_set():
            try:
                self.ser = serial.Serial(self.port, self.baud, timeout=0.1)
                self.status = f"connecté {self.port}@{self.baud}"
                if self.send_init:
                    time.sleep(0.2)
                    for cmd in AT_INIT:
                        self.ser.write(cmd)
                        time.sleep(0.15)
                    self.ser.reset_input_buffer()
                return True
            except (serial.SerialException, OSError) as e:
                self.status = f"erreur port : {e} (retry 2 s)"
                time.sleep(2.0)
        return False

    # --- parsing d'une trame complète (sans le header) -----------------
    @staticmethod
    def _parse_tlvs(frame):
        """frame = octets de la trame ENTIÈRE (header inclus). Retourne la
        liste des personnes [{id,x,y,z,vx,vy,vz,q}, ...]."""
        persons = []
        # frame[0:8]=header, [8:12]=length, [12:16]=frame_no
        data = frame[16:]
        i = 0
        n = len(data)
        while i + 8 <= n:
            tlv_type = struct.unpack_from(U32, data, i)[0]
            tlv_len = struct.unpack_from(U32, data, i + 4)[0]
            i += 8
            if tlv_len < 0 or i + tlv_len > n:
                break  # trame tronquée / désynchro
            if tlv_type == TLV_TRACKS:
                count = tlv_len // PERSON_SIZE
                for k in range(count):
                    off = i + k * PERSON_SIZE
                    q, tid, x, y, z, vx, vy, vz = struct.unpack_from(
                        "<8f", data, off
                    )
                    persons.append(dict(q=q, id=int(tid), x=x, y=y, z=z,
                                        vx=vx, vy=vy, vz=vz))
            # type 1 (point cloud, len 0) ou inconnu : on saute payload
            i += tlv_len
        return persons

    # --- boucle principale ---------------------------------------------
    def run(self):
        if not self._open():
            return
        buf = bytearray()
        while not self._stop.is_set():
            try:
                chunk = self.ser.read(4096)
            except (serial.SerialException, OSError) as e:
                self.status = f"perte port : {e}"
                self.ser = None
                if not self._open():
                    return
                buf.clear()
                continue

            if chunk:
                buf += chunk

            # consommer toutes les trames disponibles dans le buffer
            while True:
                start = buf.find(HEADER)
                if start < 0:
                    # pas de header : ne garder que la fin (header partiel possible)
                    if len(buf) > len(HEADER):
                        del buf[:-len(HEADER)]
                    break
                if start > 0:
                    del buf[:start]  # jeter le bruit avant le header (ex: "AT+OK")
                if len(buf) < MIN_FRAME_LEN:
                    break
                length = struct.unpack_from(U32, buf, 8)[0]
                if not (MIN_FRAME_LEN <= length <= MAX_FRAME_LEN):
                    # LENGTH aberrante -> resync d'un octet
                    del buf[:1]
                    continue
                if len(buf) < length:
                    break  # trame pas encore complète
                frame = bytes(buf[:length])
                del buf[:length]
                if self.debug:
                    print(f"[frame {struct.unpack_from(U32, frame, 12)[0]}] "
                          f"len={length} {frame[:24].hex(' ')} ...")
                try:
                    persons = self._parse_tlvs(frame)
                except struct.error:
                    continue
                frame_no = struct.unpack_from(U32, frame, 12)[0]
                # on ne garde que la trame la plus récente si la GUI prend du retard
                try:
                    self.q.put_nowait((frame_no, persons))
                except queue.Full:
                    try:
                        self.q.get_nowait()
                        self.q.put_nowait((frame_no, persons))
                    except queue.Empty:
                        pass

        if self.ser:
            try:
                self.ser.write(b"AT+STOP\n")
                self.ser.close()
            except Exception:
                pass


# ----------------------------------------------------------------------------
# Fenêtre principale / carte temps réel
# ----------------------------------------------------------------------------
class RadarMap(QtWidgets.QMainWindow):
    def __init__(self, reader, data_q, zone, view_m=5.0,
                 confirm=CONFIRM_FRAMES, zmin=Z_MIN, zmax=Z_MAX, zone_soft=ZONE_SOFT):
        super().__init__()
        self.reader = reader
        self.q = data_q
        self.zone = zone
        self.confirm = confirm
        self.zmin = zmin
        self.zmax = zmax
        self.zone_soft = zone_soft
        self.trails = {}          # id -> deque[(x,y)]
        self.last_seen = {}       # id -> frame_no
        self.hits = {}            # id -> nb de trames vues (confirmation anti-ghost)
        self.frame_no = 0

        self.setWindowTitle("MS72SF1 — Carte temps réel")
        self.resize(900, 900)

        pg.setConfigOptions(antialias=True, background="#101418", foreground="#d0d0d0")
        self.plot = pg.PlotWidget()
        self.setCentralWidget(self.plot)
        self.vb = self.plot.getPlotItem().getViewBox()
        self.vb.setAspectLocked(True)
        self.plot.setXRange(-view_m, view_m)
        self.plot.setYRange(-view_m, view_m)
        self.plot.showGrid(x=True, y=True, alpha=0.25)
        self.plot.setLabel("bottom", "X (m)")
        self.plot.setLabel("left", "Y (m)")

        # repère radar (origine, capteur au plafond projeté en 0,0)
        self.plot.plot([0], [0], pen=None, symbol="+", symbolSize=18,
                       symbolPen=pg.mkPen("#888", width=2), name="radar")

        # rectangle de la zone de détection configurée
        self._draw_zone()

        # items dynamiques
        self.scatter = pg.ScatterPlotItem(size=22, pen=pg.mkPen("#000", width=1))
        self.plot.addItem(self.scatter)
        self.trail_curve = pg.PlotCurveItem(pen=pg.mkPen("#5dade2", width=1.2),
                                            connect="finite")
        self.plot.addItem(self.trail_curve)
        self.vel_curve = pg.PlotCurveItem(pen=pg.mkPen("#f5b041", width=1.6),
                                          connect="pairs")
        self.plot.addItem(self.vel_curve)

        # pool d'étiquettes texte (ID) réutilisées
        self.labels = []
        for _ in range(MAX_PERSONS):
            t = pg.TextItem(color="#ffffff", anchor=(0.5, 1.6))
            t.setVisible(False)
            self.plot.addItem(t)
            self.labels.append(t)

        # barre de statut
        self.status = self.statusBar()

        # timer de rafraîchissement GUI
        self.timer = QtCore.QTimer()
        self.timer.timeout.connect(self.update_view)
        self.timer.start(40)  # 25 Hz, le radar émet ~30 ms

    def _draw_zone(self):
        z = self.zone
        x = [z["xmin"], z["xmax"], z["xmax"], z["xmin"], z["xmin"]]
        y = [z["ymin"], z["ymin"], z["ymax"], z["ymax"], z["ymin"]]
        self.plot.plot(x, y, pen=pg.mkPen("#2ecc71", width=1.5,
                                          style=QtCore.Qt.DashLine))

    def update_view(self):
        # vider la queue, ne garder que la dernière trame
        persons = None
        while True:
            try:
                self.frame_no, persons = self.q.get_nowait()
            except queue.Empty:
                break

        if persons is not None:
            self._apply_frame(persons)

        # statut (toujours rafraîchi, même sans nouvelle trame)
        self.status.showMessage(
            f"{self.reader.status}   |   trame #{self.frame_no}   |   "
            f"cibles : {len(self.trails)}"
        )

    def _filter(self, persons):
        """Filtre logiciel : rejet hors zone et hors plage de hauteur Z
        (la hauteur sert à écarter les animaux, plus bas qu'un humain debout)."""
        out = []
        for p in persons:
            if self.zone_soft:
                z = self.zone
                if not (z["xmin"] <= p["x"] <= z["xmax"]
                        and z["ymin"] <= p["y"] <= z["ymax"]):
                    continue
            if self.zmin is not None and p["z"] < self.zmin:
                continue
            if self.zmax is not None and p["z"] > self.zmax:
                continue
            out.append(p)
        return out

    def _apply_frame(self, raw_persons):
        persons = self._filter(raw_persons)

        # confirmation : compter les trames vues par piste (tue les flickers)
        for p in persons:
            tid = p["id"]
            self.last_seen[tid] = self.frame_no
            self.hits[tid] = self.hits.get(tid, 0) + 1

        confirmed = [p for p in persons
                     if self.hits.get(p["id"], 0) >= self.confirm]
        self.setWindowTitle(
            f"MS72SF1 — {len(confirmed)} confirmée(s) / {len(raw_persons)} brute(s)"
        )

        spots, vel_x, vel_y = [], [], []
        for i, p in enumerate(confirmed):
            tid, x, y = p["id"], p["x"], p["y"]
            self.trails.setdefault(tid, deque(maxlen=TRAIL_LEN)).append((x, y))

            r, g, b = color_for_id(tid)
            spots.append(dict(pos=(x, y), brush=pg.mkBrush(r, g, b),
                              size=22, pen=pg.mkPen("#000", width=1)))

            # vecteur vitesse (segment x->x+vx, y->y+vy)
            vel_x += [x, x + p["vx"] * VEL_SCALE]
            vel_y += [y, y + p["vy"] * VEL_SCALE]

            # étiquette : ID, hauteur z, qualité q, vitesse scalaire (pour calibrer)
            if i < len(self.labels):
                speed = (p["vx"] ** 2 + p["vy"] ** 2) ** 0.5
                lbl = self.labels[i]
                lbl.setText(f"#{tid}  z={p['z']:+0.2f}m  q={p['q']:0.0f}  {speed:0.1f}m/s")
                lbl.setColor(QtGui.QColor(r, g, b))
                lbl.setPos(x, y)
                lbl.setVisible(True)

        # masquer les étiquettes inutilisées
        for j in range(len(confirmed), len(self.labels)):
            self.labels[j].setVisible(False)

        self.scatter.setData(spots)
        self.vel_curve.setData(vel_x, vel_y)

        # trajectoires : une seule courbe, segments séparés par NaN
        tx, ty = [], []
        for tid, pts in self.trails.items():
            for (px, py) in pts:
                tx.append(px)
                ty.append(py)
            tx.append(np.nan)
            ty.append(np.nan)
        self.trail_curve.setData(np.array(tx), np.array(ty))

        # purge des pistes périmées (oubli + reset du compteur de confirmation)
        stale = [tid for tid, fn in self.last_seen.items()
                 if self.frame_no - fn > STALE_FRAMES]
        for tid in stale:
            self.trails.pop(tid, None)
            self.last_seen.pop(tid, None)
            self.hits.pop(tid, None)

    def closeEvent(self, ev):
        self.reader.stop()
        ev.accept()


# ----------------------------------------------------------------------------
# main
# ----------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description="Carte temps réel radar MS72SF1")
    ap.add_argument("--port", default="/dev/ttyUSB0", help="port série")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--no-init", action="store_true",
                    help="ne pas envoyer AT+DEBUG=3 / AT+START au démarrage")
    ap.add_argument("--view", type=float, default=5.0,
                    help="demi-étendue affichée en m (défaut 5)")
    ap.add_argument("--debug", action="store_true",
                    help="hexdump des trames sur stdout")
    ap.add_argument("--confirm", type=int, default=CONFIRM_FRAMES,
                    help="nb de trames avant de valider une piste (anti-ghost)")
    ap.add_argument("--zmin", type=float, default=None,
                    help="hauteur cible mini en m (rejette les cibles basses ~animaux)")
    ap.add_argument("--zmax", type=float, default=None,
                    help="hauteur cible maxi en m")
    ap.add_argument("--no-zone-filter", action="store_true",
                    help="désactive le rejet logiciel hors zone")
    args = ap.parse_args()

    data_q = queue.Queue(maxsize=4)
    reader = RadarReader(args.port, args.baud, data_q,
                         send_init=not args.no_init, debug=args.debug)
    reader.start()

    app = QtWidgets.QApplication(sys.argv)
    win = RadarMap(reader, data_q, DEFAULT_ZONE, view_m=args.view,
                   confirm=args.confirm, zmin=args.zmin, zmax=args.zmax,
                   zone_soft=not args.no_zone_filter)
    win.show()
    code = app.exec_()
    reader.stop()
    reader.join(timeout=1.0)
    sys.exit(code)


if __name__ == "__main__":
    main()
