#!/usr/bin/env python3
"""
prepare_sd.py — Przygotowanie zawartości karty microSD dla FlyRadar
=====================================================================
Źródła:
  Loga linii: github.com/Jxck-S/airline-logos (flightaware_logos, 90x90 PNG, transparent)
  Fazy księżyca: Wikimedia Commons
  Imieniny: wbudowane

Wymagania:
  pip install requests Pillow

Użycie:
  python3 prepare_sd.py [--output ./sd_card]
"""

import os, sys, shutil, argparse, io, math
import requests
from PIL import Image, ImageDraw, ImageFont

LOGO_W, LOGO_H = 120, 50
MOON_W, MOON_H =  40, 40
BG = (14, 17, 22)   # #0E1116 — uiTheme::BG
PAD = 4

HEADERS = {"User-Agent": "Mozilla/5.0 FlyRadar/2.0"}

# ─── Linie lotnicze: ICAO → nazwa ─────────────────────────────────────────────
# URL: https://raw.githubusercontent.com/Jxck-S/airline-logos/main/flightaware_logos/{ICAO}.png
# PNG 90x90, transparent background — bezpośrednie wstawianie na ciemne tło
AIRLINES = [
    "LOT","WZZ","RYR","EZY","DLH","UAE","KLM","AFR","BAW",
    "THY","SWR","AUA","QTR","FIN","SAS","IBE","CPA","ENT",
    "EXS","TAP","TVP",
]
ALIASES = {"RYA":"RYR", "JGO":"EXS", "PLL":"LOT"}

LOGO_BASE = "https://raw.githubusercontent.com/Jxck-S/airline-logos/main/flightaware_logos/{icao}.png"

# ─── Fazy księżyca ────────────────────────────────────────────────────────────
MOON_ICONS = {
    0: ("Now",             "https://upload.wikimedia.org/wikipedia/commons/thumb/a/a4/New_moon.png/240px-New_moon.png"),
    1: ("Przybyw.sierp",  "https://upload.wikimedia.org/wikipedia/commons/thumb/6/6a/Waxing_crescent_moon.png/240px-Waxing_crescent_moon.png"),
    2: ("Pierwsza kwadra","https://upload.wikimedia.org/wikipedia/commons/thumb/8/81/First_quarter_moon.png/240px-First_quarter_moon.png"),
    3: ("Przybyw.garb",   "https://upload.wikimedia.org/wikipedia/commons/thumb/7/7a/Waxing_gibbous_moon.png/240px-Waxing_gibbous_moon.png"),
    4: ("Pelnia",         "https://upload.wikimedia.org/wikipedia/commons/thumb/4/40/Full_moon.jpeg/240px-Full_moon.jpeg"),
    5: ("Ubyw.garb",      "https://upload.wikimedia.org/wikipedia/commons/thumb/2/2b/Waning_gibbous_moon.png/240px-Waning_gibbous_moon.png"),
    6: ("Ostatnia kwadra","https://upload.wikimedia.org/wikipedia/commons/thumb/c/c7/Last_quarter_moon.png/240px-Last_quarter_moon.png"),
    7: ("Ubyw.sierp",     "https://upload.wikimedia.org/wikipedia/commons/thumb/8/85/Waning_crescent_moon.png/240px-Waning_crescent_moon.png"),
}

# ─── Polskie imieniny ─────────────────────────────────────────────────────────
NAMEDAYS = {
    (1,1):"Mikolaja",(1,2):"Abla",(1,3):"Genowefy",(1,4):"Tytusa",
    (1,5):"Szymona",(1,6):"Melchiora",(1,7):"Juliana",(1,8):"Seweryna",
    (1,9):"Marceliny",(1,10):"Agnieszki",(1,11):"Honoraty",(1,12):"Benedykta",
    (1,13):"Hilarego",(1,14):"Feliksa",(1,15):"Pawla",(1,16):"Marcelego",
    (1,17):"Antoniego",(1,18):"Piotra",(1,19):"Henryka",(1,20):"Fabiana",
    (1,21):"Agnieszki",(1,22):"Anastazego",(1,23):"Ildefonsa",(1,24):"Tymoteusza",
    (1,25):"Pawla",(1,26):"Polkarpa",(1,27):"Jana",(1,28):"Karola",
    (1,29):"Franciszki",(1,30):"Martyny",(1,31):"Norberta",
    (2,1):"Brygidy",(2,2):"Marii",(2,3):"Blazeja",(2,4):"Andrzeja",
    (2,5):"Agaty",(2,6):"Doroty",(2,7):"Ryszarda",(2,8):"Hieronima",
    (2,9):"Apolonii",(2,10):"Scholastyki",(2,11):"Bernarda",(2,12):"Eulalii",
    (2,13):"Grzegorza",(2,14):"Walentego",(2,15):"Faustyna",(2,16):"Danuty",
    (2,17):"Lukasza",(2,18):"Konstancji",(2,19):"Konrada",(2,20):"Leona",
    (2,21):"Eleonory",(2,22):"Marty",(2,23):"Romana",(2,24):"Macieja",
    (2,25):"Wiktora",(2,26):"Aleksandra",(2,27):"Gabriela",(2,28):"Bozeny",
    (3,1):"Albina",(3,2):"Heleny",(3,3):"Kunegundy",(3,4):"Kazimierza",
    (3,5):"Adriana",(3,6):"Rozy",(3,7):"Tomasza",(3,8):"Beaty",
    (3,9):"Franciszki",(3,10):"Cypriana",(3,11):"Konstantego",(3,12):"Grzegorza",
    (3,13):"Krystyny",(3,14):"Matyldy",(3,15):"Longina",(3,16):"Hilarego",
    (3,17):"Patryka",(3,18):"Cyryla",(3,19):"Jozefa",(3,20):"Klaudiusza",
    (3,21):"Benedykta",(3,22):"Katarzyny",(3,23):"Pelagii",(3,24):"Marka",
    (3,25):"Marii",(3,26):"Emanuela",(3,27):"Gabriela",(3,28):"Sykstusa",
    (3,29):"Jana",(3,30):"Kwiryna",(3,31):"Beniamina",
    (4,1):"Hugona",(4,2):"Franciszki",(4,3):"Ryszarda",(4,4):"Izydora",
    (4,5):"Wincentego",(4,6):"Wilhelma",(4,7):"Donata",(4,8):"Dionizego",
    (4,9):"Marii",(4,10):"Michala",(4,11):"Leona",(4,12):"Juliusza",
    (4,13):"Hermenegildy",(4,14):"Bernadety",(4,15):"Anastazji",(4,16):"Benedykta",
    (4,17):"Aniceta",(4,18):"Apolloniusza",(4,19):"Leona",(4,20):"Czeslawa",
    (4,21):"Anzelma",(4,22):"Leona",(4,23):"Wojciecha",(4,24):"Grzegorza",
    (4,25):"Marka",(4,26):"Marzeny",(4,27):"Zity",(4,28):"Walerii",
    (4,29):"Piotra",(4,30):"Katarzyny",
    (5,1):"Jozefa",(5,2):"Atanazego",(5,3):"Marii",(5,4):"Moniki",
    (5,5):"Ireny",(5,6):"Jana",(5,7):"Gizeli",(5,8):"Stanislawa",
    (5,9):"Grzegorza",(5,10):"Jana",(5,11):"Igi",(5,12):"Dominika",
    (5,13):"Serwacego",(5,14):"Bonifacego",(5,15):"Zofii",(5,16):"Andrzeja",
    (5,17):"Paschalisa",(5,18):"Jana",(5,19):"Piotra",(5,20):"Bernardyna",
    (5,21):"Wiktora",(5,22):"Romy",(5,23):"Dezyderego",(5,24):"Jana",
    (5,25):"Grzegorza",(5,26):"Filipa",(5,27):"Jana",(5,28):"Germana",
    (5,29):"Benedykty",(5,30):"Ferdynanda",(5,31):"Anieli",
    (6,1):"Justyny",(6,2):"Marcelina",(6,3):"Klotyldy",(6,4):"Franciszka",
    (6,5):"Bonifacego",(6,6):"Norberta",(6,7):"Roberta",(6,8):"Medarda",
    (6,9):"Felicjana",(6,10):"Malgorzaty",(6,11):"Barnaby",(6,12):"Onufrego",
    (6,13):"Antoniego",(6,14):"Bazylego",(6,15):"Wita",(6,16):"Benona",
    (6,17):"Marcina",(6,18):"Marka",(6,19):"Gerwazego",(6,20):"Bogumily",
    (6,21):"Alojzego",(6,22):"Pauliny",(6,23):"Wandy",(6,24):"Jana",
    (6,25):"Lucji",(6,26):"Jana",(6,27):"Ladyslawa",(6,28):"Leona",
    (6,29):"Piotra",(6,30):"Pawla",
    (7,1):"Haliny",(7,2):"Urbana",(7,3):"Anatola",(7,4):"Ulryka",
    (7,5):"Antoniego",(7,6):"Dominiki",(7,7):"Cyryla",(7,8):"Elzbiety",
    (7,9):"Weroniki",(7,10):"Amelii",(7,11):"Benedykta",(7,12):"Jana",
    (7,13):"Henryka",(7,14):"Kamila",(7,15):"Henryka",(7,16):"Eustachego",
    (7,17):"Aleksego",(7,18):"Szymona",(7,19):"Wincentego",(7,20):"Hieronima",
    (7,21):"Danuty",(7,22):"Marii",(7,23):"Brygidy",(7,24):"Kingi",
    (7,25):"Krzysztofa",(7,26):"Anny",(7,27):"Natalii",(7,28):"Wiktora",
    (7,29):"Marty",(7,30):"Piotra",(7,31):"Ignacego",
    (8,1):"Alfonsa",(8,2):"Gustawa",(8,3):"Lidii",(8,4):"Dominiki",
    (8,5):"Oswalda",(8,6):"Sykstusa",(8,7):"Kajetana",(8,8):"Cyriaka",
    (8,9):"Romana",(8,10):"Wawrzynca",(8,11):"Klary",(8,12):"Klary",
    (8,13):"Hipolita",(8,14):"Maksymiliana",(8,15):"Marii",(8,16):"Rocha",
    (8,17):"Jacka",(8,18):"Heleny",(8,19):"Jana",(8,20):"Bernarda",
    (8,21):"Piusa",(8,22):"Marii",(8,23):"Filipa",(8,24):"Bartlomieja",
    (8,25):"Ludwika",(8,26):"Zefiryna",(8,27):"Cesarego",(8,28):"Augustyna",
    (8,29):"Jana",(8,30):"Rozy",(8,31):"Rajmunda",
    (9,1):"Bronislawy",(9,2):"Stefana",(9,3):"Grzegorza",(9,4):"Rozalii",
    (9,5):"Wawrzynca",(9,6):"Beaty",(9,7):"Reginy",(9,8):"Marii",
    (9,9):"Sergiusza",(9,10):"Pulcherii",(9,11):"Jacka",(9,12):"Gwidona",
    (9,13):"Jana",(9,14):"Walentego",(9,15):"Marii",(9,16):"Edyty",
    (9,17):"Roberta",(9,18):"Jozefa",(9,19):"Januarego",(9,20):"Eustachego",
    (9,21):"Mateusza",(9,22):"Maurycego",(9,23):"Tekli",(9,24):"Rozalii",
    (9,25):"Wladyslawa",(9,26):"Cypriana",(9,27):"Wincentego",(9,28):"Waclawy",
    (9,29):"Michala",(9,30):"Hieronima",
    (10,1):"Remigiusza",(10,2):"Leandra",(10,3):"Gerarda",(10,4):"Franciszka",
    (10,5):"Placyda",(10,6):"Brunona",(10,7):"Justyny",(10,8):"Brygidy",
    (10,9):"Dionizego",(10,10):"Franciszka",(10,11):"Marii",(10,12):"Maksymiliana",
    (10,13):"Edwarda",(10,14):"Kalixta",(10,15):"Teresy",(10,16):"Jadwigi",
    (10,17):"Ignacego",(10,18):"Lukasza",(10,19):"Jana",(10,20):"Ireny",
    (10,21):"Urszuli",(10,22):"Filipy",(10,23):"Jana",(10,24):"Rafala",
    (10,25):"Darii",(10,26):"Ewarysta",(10,27):"Sabiny",(10,28):"Szymona",
    (10,29):"Narcyza",(10,30):"Zenobii",(10,31):"Wolfganga",
    (11,1):"Wszystkich Swietych",(11,2):"Bogumily",(11,3):"Huberta",(11,4):"Karola",
    (11,5):"Slawomiry",(11,6):"Leonarda",(11,7):"Ernesta",(11,8):"Klaudiusza",
    (11,9):"Teodora",(11,10):"Leona",(11,11):"Marcina",(11,12):"Renaty",
    (11,13):"Stanislawa",(11,14):"Rogera",(11,15):"Alberta",(11,16):"Edmunda",
    (11,17):"Grzegorza",(11,18):"Romana",(11,19):"Elzbiety",(11,20):"Feliksa",
    (11,21):"Geralda",(11,22):"Cecylii",(11,23):"Klemensa",(11,24):"Jana",
    (11,25):"Katarzyny",(11,26):"Leonarda",(11,27):"Wirgiliusza",(11,28):"Zdzislawa",
    (11,29):"Saturnina",(11,30):"Andrzeja",
    (12,1):"Eligiusza",(12,2):"Bianki",(12,3):"Franciszki",(12,4):"Barbary",
    (12,5):"Kryspiny",(12,6):"Mikolaja",(12,7):"Ambrozego",(12,8):"Marii",
    (12,9):"Wieslawy",(12,10):"Julii",(12,11):"Damazego",(12,12):"Aleksandra",
    (12,13):"Lucji",(12,14):"Jana",(12,15):"Niny",(12,16):"Zuzanny",
    (12,17):"Lazarza",(12,18):"Gracjana",(12,19):"Urbana",(12,20):"Boguslawa",
    (12,21):"Tomasza",(12,22):"Zenona",(12,23):"Jana",(12,24):"Adama",
    (12,25):"Bozego Narodzenia",(12,26):"Szczepana",(12,27):"Jana",
    (12,28):"Mlodych Meczennikow",(12,29):"Tomasza",(12,30):"Dawida",
    (12,31):"Sylwestra",
}

# ─── Helpers ──────────────────────────────────────────────────────────────────
HEADERS = {"User-Agent": "Mozilla/5.0 FlyRadar/2.0 (ESP32 home project)"}

def fetch(url: str) -> bytes | None:
    try:
        r = requests.get(url, timeout=15, headers=HEADERS)
        if r.status_code == 200 and len(r.content) > 200:
            return r.content
        print(f"    HTTP {r.status_code}: {url.split('/')[-1]}")
    except Exception as e:
        print(f"    ERR: {e}")
    return None

def make_logo(data: bytes | None, icao: str) -> Image.Image:
    """PNG z transparentnym tłem → 120×50 na ciemnym tle."""
    out = Image.new("RGB", (LOGO_W, LOGO_H), BG)
    if data is None:
        draw = ImageDraw.Draw(out)
        try:    font = ImageFont.truetype("/System/Library/Fonts/Helvetica.ttc", 11)
        except: font = ImageFont.load_default()
        bb = draw.textbbox((0,0), icao, font=font)
        tw, th = bb[2]-bb[0], bb[3]-bb[1]
        draw.text(((LOGO_W-tw)//2, (LOGO_H-th)//2), icao, fill=(90,100,115), font=font)
        return out
    try:
        img = Image.open(io.BytesIO(data)).convert("RGBA")
    except Exception as e:
        print(f"    PIL err: {e}"); return out

    # Skomponuj na ciemnym tle (przezroczysty PNG → bez białej ramki)
    bg = Image.new("RGBA", img.size, (*BG, 255))
    bg.paste(img, mask=img.split()[3])
    img_rgb = bg.convert("RGB")

    # Skaluj do 120×50 z paddingiem, zachowaj proporcje
    img_rgb.thumbnail((LOGO_W - 2*PAD, LOGO_H - 2*PAD), Image.LANCZOS)
    x = (LOGO_W - img_rgb.width) // 2
    y = (LOGO_H - img_rgb.height) // 2
    out.paste(img_rgb, (x, y))
    return out

def make_moon(data: bytes | None, name: str) -> Image.Image:
    """
    Generuje ikonkę fazy księżyca geometrycznie (emoji-style).
    Ignoruje 'data' z internetu — zawsze rysuje lokalnie.
    'name' to nieużywany parametr (kompatybilność ze starym kodem).
    
    Wywołuj przez generate_moon_icons() poniżej.
    """
    return Image.new('RGB', (MOON_W, MOON_H), BG)

# Kąt oświetlenia dla każdej fazy (0°=nów, 180°=pełnia)
MOON_PHASE_ANGLES = {
    0: 0,    # 🌑 nów
    1: 45,   # 🌒 przybywający sierp
    2: 90,   # 🌓 pierwsza kwadra
    3: 135,  # 🌔 przybywający garb
    4: 180,  # 🌕 pełnia
    5: 225,  # 🌖 ubywający garb
    6: 270,  # 🌗 ostatnia kwadra
    7: 315,  # 🌘 ubywający sierp
}

def _is_lit(x: float, y: float, angle_deg: float, cx: float, cy: float, r: float) -> bool:
    """Czy subpiksel (x,y) jest oświetlony przy kącie terminatora angle_deg?"""
    dx = (x - cx) / r
    dy = (y - cy) / r
    if dx*dx + dy*dy > 1.0:
        return False
    ey = math.sqrt(max(0.0, 1.0 - dy*dy))
    if angle_deg <= 180:
        term = math.cos(math.radians(angle_deg))
        return dx > term * ey
    else:
        term = math.cos(math.radians(360 - angle_deg))
        return dx < -term * ey

def generate_moon_icon(phase_idx: int) -> Image.Image:
    """Generuje 40×40 BMP fazy księżyca — renderowanie geometryczne z antyaliasingiem."""
    LIGHT  = (235, 225, 190)
    SCALE  = 8
    angle  = MOON_PHASE_ANGLES[phase_idx]
    cx, cy = MOON_W / 2.0, MOON_H / 2.0
    r      = MOON_W / 2.0 - 2

    lit = [[0] * MOON_W for _ in range(MOON_H)]
    for spy in range(MOON_H * SCALE):
        for spx in range(MOON_W * SCALE):
            if _is_lit((spx + 0.5) / SCALE, (spy + 0.5) / SCALE, angle, cx, cy, r):
                lit[spy // SCALE][spx // SCALE] += 1

    total = SCALE * SCALE
    img = Image.new('RGB', (MOON_W, MOON_H), BG)
    px = img.load()
    for py in range(MOON_H):
        for ppx in range(MOON_W):
            t = lit[py][ppx] / total
            px[ppx, py] = (
                int(BG[0] + (LIGHT[0] - BG[0]) * t),
                int(BG[1] + (LIGHT[1] - BG[1]) * t),
                int(BG[2] + (LIGHT[2] - BG[2]) * t),
            )
    ImageDraw.Draw(img).ellipse(
        [cx-r, cy-r, cx+r-1, cy+r-1], outline=(80, 75, 65), width=1)
    return img

def save_bmp(img: Image.Image, path: str):
    img.convert("RGB").save(path, format="BMP")

# ─── Główna logika ─────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default="./sd_card")
    parser.add_argument("--skip-logos", action="store_true")
    parser.add_argument("--skip-moon",  action="store_true")
    args = parser.parse_args()

    airlines_dir = os.path.join(args.output, "airlines")
    moon_dir     = os.path.join(args.output, "moon")
    os.makedirs(airlines_dir, exist_ok=True)
    os.makedirs(moon_dir, exist_ok=True)

    print(f"\n{'='*60}")
    print(f"  FlyRadar SD Card Prep  →  {os.path.abspath(args.output)}")
    print(f"  Zrodlo logo: github.com/Jxck-S/airline-logos (flightaware)")
    print(f"{'='*60}")

    # ── 1. Loga linii ─────────────────────────────────────────────────────────
    if not args.skip_logos:
        print("\n[ LOGA LINII LOTNICZYCH  120x50 BMP ]")
        ok = 0
        for icao in AIRLINES:
            url  = LOGO_BASE.format(icao=icao)
            path = os.path.join(airlines_dir, f"{icao}.bmp")
            print(f"  {icao:<6}", end=" ", flush=True)
            data = fetch(url)
            save_bmp(make_logo(data, icao), path)
            if data:
                print("OK")
                ok += 1
            else:
                print("placeholder")
        print(f"\n  Aliasy (kopie):")
        for alias, src in ALIASES.items():
            dst = os.path.join(airlines_dir, f"{alias}.bmp")
            src_path = os.path.join(airlines_dir, f"{src}.bmp")
            if os.path.exists(src_path):
                shutil.copy2(src_path, dst)
                print(f"  {alias}.bmp  =  {src}.bmp")
        print(f"\n  {ok}/{len(AIRLINES)} logo pobranych")

    # ── 2. Fazy księżyca — generowane lokalnie (bez internetu) ───────────────
    if not args.skip_moon:
        print("\n[ IKONKI KSIEZYCA  40x40 BMP  (generowane geometrycznie) ]")
        for idx in range(8):
            path = os.path.join(moon_dir, f"{idx}.bmp")
            img = generate_moon_icon(idx)
            save_bmp(img, path)
            name = list(MOON_ICONS.values())[idx][0]
            print(f"  {idx} {name:<20} OK  (kąt {MOON_PHASE_ANGLES[idx]}°)")

    # ── 3. namedays.csv ───────────────────────────────────────────────────────
    print("\n[ NAMEDAYS.CSV ]")
    csv_path = os.path.join(args.output, "namedays.csv")
    with open(csv_path, "w", encoding="utf-8") as f:
        for (month, day), names in sorted(NAMEDAYS.items()):
            f.write(f"{month:02d},{day:02d},{names}\n")
    print(f"  {len(NAMEDAYS)} wpisow → {csv_path}")

    # ── 4. family.csv — personal family events ───────────────────────────────
    # Edit this list with your own dates before running the script.
    # This section is intentionally left with generic examples in the public repo.
    print("\n[ FAMILY.CSV — personal family events ]")
    family_events = [
        # (month, day, "Description")
        # Add your own dates here:
        (1, 21, "Grandparents Day"),
        (2, 14, "Valentines Day"),
        (5, 26, "Mothers Day"),
        (6,  1, "Childrens Day"),
        (6,  6, "Anniversary"),
        (12, 24, "Christmas Eve"),
        (12, 25, "Christmas"),
        (12, 31, "New Years Eve"),
    ]
    fam_path = os.path.join(args.output, "family.csv")
    with open(fam_path, "w", encoding="utf-8") as f:
        for mo, da, desc in family_events:
            f.write(f"{mo:02d},{da:02d},{desc}\n")
    print(f"  {len(family_events)} swiat → {fam_path}")

    # ── 5. holidays.csv — popularne swieta polskie ────────────────────────────
    print("\n[ HOLIDAYS.CSV — popularne swieta polskie ]")
    holidays = [
        (1,  1,  "Nowy Rok"),
        (1,  6,  "Trzech Kroli"),
        (2,  2,  "Gromniczna"),
        (2,  14, "Walentynki"),
        (3,  21, "Pierwszy dzien wiosny"),
        (4,  23, "Swiety Jerzy"),
        (5,  1,  "Swieto Pracy"),
        (5,  2,  "Dzien Flagi"),
        (5,  3,  "Konstytucja 3 Maja"),
        (5,  12, "Zimni Ogrodnicy - Pankracy"),
        (5,  13, "Zimni Ogrodnicy - Serwacy"),
        (5,  14, "Zimni Ogrodnicy - Bonifacy"),
        (5,  15, "Zimna Zofja"),
        (6,  21, "Noc Kupaly - Sobotka"),
        (6,  23, "Wianki"),
        (8,  15, "Wniebowziecie NMP"),
        (10, 1,  "Dzien Nauczyciela"),
        (11, 1,  "Wszystkich Swietych"),
        (11, 2,  "Dzien Zaduszny"),
        (11, 11, "Swieto Niepodleglosci"),
        (12, 6,  "Swiety Mikolaj"),
        (12, 24, "Wigilia Bozego Narodzenia"),
        (12, 25, "Boze Narodzenie"),
        (12, 26, "Drugi dzien Swiat"),
        (12, 31, "Sylwester"),
    ]
    hol_path = os.path.join(args.output, "holidays.csv")
    with open(hol_path, "w", encoding="utf-8") as f:
        for mo, da, desc in holidays:
            f.write(f"{mo:02d},{da:02d},{desc}\n")
    print(f"  {len(holidays)} swiat → {hol_path}")

    # ── Podsumowanie ──────────────────────────────────────────────────────────
    total_kb = sum(
        os.path.getsize(os.path.join(root, f))
        for root, _, files in os.walk(args.output)
        for f in files
    ) // 1024
    print(f"\n{'='*60}")
    print(f"  Gotowe! Skopiuj na karte SD:")
    print(f"    {os.path.abspath(args.output)}/")
    print(f"      airlines/    — {len(os.listdir(airlines_dir))} plikow BMP")
    print(f"      moon/        — {len(os.listdir(moon_dir))} plikow BMP")
    print(f"      namedays.csv — imieniny")
    print(f"      family.csv   — {len(family_events)} swiat rodzinnych")
    print(f"      holidays.csv — {len(holidays)} popularnych swiat")
    print(f"  Lacznie: ~{total_kb} KB")
    print(f"{'='*60}\n")

if __name__ == "__main__":
    main()
