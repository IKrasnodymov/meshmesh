#!/usr/bin/env python3
"""The device page with made-up data, for screenshots of the site (tools/web_shots.py) and for looking at
the page in any language without a board: web/index.html, its dictionaries (i18n/web) and /api/* answers
of an imaginary M9 with contacts, channels, messages, a chess game, checkers and sea battle and the radar.
Nothing here comes from a real device: names, keys, places and messages are invented.

    python3 tools/web_demo.py [--port 8790]      then open http://127.0.0.1:8790/?lang=de#chats

Checkers and sea battle are fixed games without --duel. With --duel PROGRAM (tools/ui_preview/build.sh with
KEEP=PROGRAM builds it) the real src/Duel.cpp plays them: one copy is this board, a second one answers as Bob V4
(accepts, moves a random legal move, shoots at random) and the demo carries their messages both ways.
"""
import argparse
import json
import random
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

ROOT = Path(__file__).resolve().parent.parent
VERSION = dict(l.split('=', 1) for l in (ROOT/'versions.properties').read_text().split('\n') if '=' in l and not l.startswith('#'))['firmware'].strip()
LANGS = ['en', 'uk', 'es', 'pt', 'fr', 'de', 'it', 'pl', 'tr', 'zh', 'ja', 'ko', 'ar', 'id']
NOW = 1791800000  # a fixed clock: the same pictures every time
ME = '5A7C21E0B4D3F190'
HOME = (46.0207, 7.7491)  # an imaginary valley

# Messages of the demo in each language: (from, text, minutes ago). "me" sends.
CHATS = {
    'ru': {'bob': [('bob', 'Радиотест V4 → M9, как слышно?', 62), ('me', 'Принято, слышу хорошо. SNR около 9 дБ', 60), ('bob', 'Отлично. Поднимусь на холм и проверю дальность', 59), ('me', 'Давай. Я пока объявлю узел и передам позицию', 57), ('bob', 'С холма вижу ретранслятор напрямую, −83 dBm', 25), ('me', 'Супер, сообщения доходят с первой попытки', 24), ('bob', 'Возвращаюсь. Встретимся у входа через 10 минут', 7), ('me', 'Договорились, жду у входа', 6)],
           'pub': [('anna', 'Всем привет! Кто на связи у озера?', 40), ('me', 'Я на связи, M9 у моста', 38), ('rita', 'Слышу вас через North Hill', 30), ('anna', 'До вечера!', 2)]},
    'en': {'bob': [('bob', 'Radio test V4 → M9, how do you copy?', 62), ('me', 'Copy, loud and clear. SNR about 9 dB', 60), ('bob', 'Great. Going up the hill to check the range', 59), ('me', 'Go ahead. I will advert the node and share my position', 57), ('bob', 'From the hill I see the repeater directly, −83 dBm', 25), ('me', 'Nice, messages get through on the first try', 24), ('bob', 'Heading back. Meet at the entrance in 10 minutes', 7), ('me', 'Deal, waiting at the entrance', 6)],
           'pub': [('anna', 'Hi all! Anyone on air near the lake?', 40), ('me', 'Here, M9 by the bridge', 38), ('rita', 'Hearing you through North Hill', 30), ('anna', 'See you tonight!', 2)]},
    'uk': {'bob': [('bob', 'Радіотест V4 → M9, як чути?', 62), ('me', 'Прийнято, чую добре. SNR близько 9 дБ', 60), ('bob', 'Чудово. Піднімуся на пагорб і перевірю дальність', 59), ('me', 'Давай. Я поки оголошу вузол і передам позицію', 57), ('bob', 'З пагорба бачу ретранслятор напряму, −83 dBm', 25), ('me', 'Супер, повідомлення доходять з першої спроби', 24), ('bob', 'Повертаюся. Зустрінемося біля входу за 10 хвилин', 7), ('me', 'Домовились, чекаю біля входу', 6)],
           'pub': [('anna', 'Всім привіт! Хто на зв’язку біля озера?', 40), ('me', 'Я на зв’язку, M9 біля мосту', 38), ('rita', 'Чую вас через North Hill', 30), ('anna', 'До вечора!', 2)]},
    'es': {'bob': [('bob', 'Prueba de radio V4 → M9, ¿cómo me recibes?', 62), ('me', 'Recibido, te oigo bien. SNR de unos 9 dB', 60), ('bob', 'Genial. Subo a la colina a probar el alcance', 59), ('me', 'Vale. Anuncio el nodo y envío mi posición', 57), ('bob', 'Desde la colina veo el repetidor directo, −83 dBm', 25), ('me', 'Perfecto, los mensajes llegan al primer intento', 24), ('bob', 'Vuelvo. Nos vemos en la entrada en 10 minutos', 7), ('me', 'Hecho, te espero en la entrada', 6)],
           'pub': [('anna', '¡Hola a todos! ¿Alguien en el aire cerca del lago?', 40), ('me', 'Aquí, M9 junto al puente', 38), ('rita', 'Os oigo a través de North Hill', 30), ('anna', '¡Hasta la noche!', 2)]},
    'pt': {'bob': [('bob', 'Teste de rádio V4 → M9, como está me ouvindo?', 62), ('me', 'Recebido, ouço bem. SNR por volta de 9 dB', 60), ('bob', 'Ótimo. Vou subir o morro para testar o alcance', 59), ('me', 'Beleza. Vou anunciar o nó e enviar a posição', 57), ('bob', 'Do morro vejo o repetidor direto, −83 dBm', 25), ('me', 'Show, as mensagens chegam na primeira tentativa', 24), ('bob', 'Voltando. Nos vemos na entrada em 10 minutos', 7), ('me', 'Combinado, te espero na entrada', 6)],
           'pub': [('anna', 'Oi, pessoal! Alguém no ar perto do lago?', 40), ('me', 'Aqui, M9 perto da ponte', 38), ('rita', 'Ouço vocês pelo North Hill', 30), ('anna', 'Até a noite!', 2)]},
    'fr': {'bob': [('bob', 'Essai radio V4 → M9, tu me reçois ?', 62), ('me', 'Reçu, je t’entends bien. SNR vers 9 dB', 60), ('bob', 'Super. Je monte sur la colline tester la portée', 59), ('me', 'Vas-y. J’annonce le nœud et j’envoie ma position', 57), ('bob', 'De la colline je vois le répéteur en direct, −83 dBm', 25), ('me', 'Parfait, les messages passent du premier coup', 24), ('bob', 'Je reviens. Rendez-vous à l’entrée dans 10 minutes', 7), ('me', 'Ça marche, je t’attends à l’entrée', 6)],
           'pub': [('anna', 'Salut tout le monde ! Quelqu’un près du lac ?', 40), ('me', 'Présent, M9 près du pont', 38), ('rita', 'Je vous entends via North Hill', 30), ('anna', 'À ce soir !', 2)]},
    'de': {'bob': [('bob', 'Funktest V4 → M9, wie ist der Empfang?', 62), ('me', 'Verstanden, gut lesbar. SNR etwa 9 dB', 60), ('bob', 'Prima. Ich gehe auf den Hügel und teste die Reichweite', 59), ('me', 'Mach das. Ich kündige den Knoten an und sende meine Position', 57), ('bob', 'Vom Hügel sehe ich den Repeater direkt, −83 dBm', 25), ('me', 'Super, die Nachrichten kommen beim ersten Versuch an', 24), ('bob', 'Ich komme zurück. Treffen am Eingang in 10 Minuten', 7), ('me', 'Abgemacht, ich warte am Eingang', 6)],
           'pub': [('anna', 'Hallo zusammen! Ist jemand am See auf Sendung?', 40), ('me', 'Hier, M9 an der Brücke', 38), ('rita', 'Höre euch über North Hill', 30), ('anna', 'Bis heute Abend!', 2)]},
    'it': {'bob': [('bob', 'Prova radio V4 → M9, come mi ricevi?', 62), ('me', 'Ricevuto, ti sento bene. SNR circa 9 dB', 60), ('bob', 'Ottimo. Salgo sulla collina a provare la portata', 59), ('me', 'Vai. Intanto annuncio il nodo e invio la posizione', 57), ('bob', 'Dalla collina vedo il ripetitore in diretta, −83 dBm', 25), ('me', 'Perfetto, i messaggi arrivano al primo tentativo', 24), ('bob', 'Torno indietro. Ci vediamo all’ingresso tra 10 minuti', 7), ('me', 'D’accordo, ti aspetto all’ingresso', 6)],
           'pub': [('anna', 'Ciao a tutti! Qualcuno in aria vicino al lago?', 40), ('me', 'Ci sono, M9 vicino al ponte', 38), ('rita', 'Vi sento tramite North Hill', 30), ('anna', 'A stasera!', 2)]},
    'pl': {'bob': [('bob', 'Test radia V4 → M9, jak mnie słychać?', 62), ('me', 'Odebrane, słyszę dobrze. SNR około 9 dB', 60), ('bob', 'Świetnie. Wejdę na wzgórze i sprawdzę zasięg', 59), ('me', 'Dawaj. Ogłoszę węzeł i wyślę pozycję', 57), ('bob', 'Ze wzgórza widzę przekaźnik bezpośrednio, −83 dBm', 25), ('me', 'Super, wiadomości dochodzą za pierwszym razem', 24), ('bob', 'Wracam. Spotkajmy się przy wejściu za 10 minut', 7), ('me', 'Umowa, czekam przy wejściu', 6)],
           'pub': [('anna', 'Cześć wszystkim! Ktoś na nasłuchu przy jeziorze?', 40), ('me', 'Jestem, M9 przy moście', 38), ('rita', 'Słyszę was przez North Hill', 30), ('anna', 'Do wieczora!', 2)]},
    'tr': {'bob': [('bob', 'Telsiz testi V4 → M9, nasıl duyuyorsun?', 62), ('me', 'Alındı, iyi duyuyorum. SNR yaklaşık 9 dB', 60), ('bob', 'Harika. Tepeye çıkıp menzili deneyeceğim', 59), ('me', 'Tamam. Ben düğümü duyurup konumumu gönderiyorum', 57), ('bob', 'Tepeden tekrarlayıcıyı doğrudan görüyorum, −83 dBm', 25), ('me', 'Süper, mesajlar ilk denemede ulaşıyor', 24), ('bob', 'Dönüyorum. 10 dakika sonra girişte buluşalım', 7), ('me', 'Anlaştık, girişte bekliyorum', 6)],
           'pub': [('anna', 'Herkese selam! Gölün yanında yayında olan var mı?', 40), ('me', 'Buradayım, köprünün yanında M9', 38), ('rita', 'Sizi North Hill üzerinden duyuyorum', 30), ('anna', 'Akşam görüşürüz!', 2)]},
    'zh': {'bob': [('bob', '无线电测试 V4 → M9，收到吗？', 62), ('me', '收到，信号很好。SNR 约 9 dB', 60), ('bob', '太好了。我上山测一下距离', 59), ('me', '好的。我先通告节点并发送位置', 57), ('bob', '在山上能直接看到中继器，−83 dBm', 25), ('me', '很好，消息一次就送达了', 24), ('bob', '我回来了。10 分钟后入口见', 7), ('me', '好的，在入口等你', 6)],
           'pub': [('anna', '大家好！湖边有人在线吗？', 40), ('me', '我在，M9 在桥边', 38), ('rita', '通过 North Hill 能听到你们', 30), ('anna', '晚上见！', 2)]},
    'ja': {'bob': [('bob', '無線テスト V4 → M9、聞こえますか？', 62), ('me', '了解、よく聞こえます。SNR は約 9 dB', 60), ('bob', 'いいですね。丘に登って距離を確かめます', 59), ('me', 'どうぞ。ノードをアドバートして位置を送ります', 57), ('bob', '丘からリピーターが直接見えます、−83 dBm', 25), ('me', 'メッセージが一回で届いています', 24), ('bob', '戻ります。10 分後に入口で', 7), ('me', '了解、入口で待っています', 6)],
           'pub': [('anna', 'みなさん、こんにちは！湖の近くに誰かいますか？', 40), ('me', 'います、橋のそばの M9 です', 38), ('rita', 'North Hill 経由で聞こえます', 30), ('anna', 'また夜に！', 2)]},
    'ko': {'bob': [('bob', '무선 테스트 V4 → M9, 잘 들려요?', 62), ('me', '수신, 잘 들려요. SNR 약 9 dB', 60), ('bob', '좋아요. 언덕에 올라가서 거리를 확인할게요', 59), ('me', '네. 노드를 알리고 위치를 보낼게요', 57), ('bob', '언덕에서 리피터가 바로 보여요, −83 dBm', 25), ('me', '메시지가 한 번에 도착해요', 24), ('bob', '돌아갈게요. 10분 뒤 입구에서 만나요', 7), ('me', '좋아요, 입구에서 기다릴게요', 6)],
           'pub': [('anna', '모두 안녕하세요! 호수 근처에 계신 분?', 40), ('me', '여기요, 다리 옆 M9', 38), ('rita', 'North Hill을 통해 들려요', 30), ('anna', '저녁에 봐요!', 2)]},
    'ar': {'bob': [('bob', 'اختبار لاسلكي V4 → M9، كيف تسمعني؟', 62), ('me', 'تم الاستلام، أسمعك جيدا. SNR نحو 9 dB', 60), ('bob', 'ممتاز. سأصعد التلة لأختبر المدى', 59), ('me', 'حسنا. سأعلن العقدة وأرسل موقعي', 57), ('bob', 'من التلة أرى المرحل مباشرة، −83 dBm', 25), ('me', 'رائع، الرسائل تصل من أول محاولة', 24), ('bob', 'أنا عائد. نلتقي عند المدخل بعد 10 دقائق', 7), ('me', 'اتفقنا، أنتظرك عند المدخل', 6)],
           'pub': [('anna', 'مرحبا بالجميع! هل من أحد قرب البحيرة؟', 40), ('me', 'أنا هنا، M9 قرب الجسر', 38), ('rita', 'أسمعكم عبر North Hill', 30), ('anna', 'إلى اللقاء مساء!', 2)]},
    'id': {'bob': [('bob', 'Tes radio V4 → M9, terdengar?', 62), ('me', 'Diterima, terdengar jelas. SNR sekitar 9 dB', 60), ('bob', 'Mantap. Saya naik bukit untuk cek jangkauan', 59), ('me', 'Silakan. Saya umumkan node dan kirim posisi', 57), ('bob', 'Dari bukit repeater terlihat langsung, −83 dBm', 25), ('me', 'Bagus, pesan sampai pada percobaan pertama', 24), ('bob', 'Saya kembali. Ketemu di pintu masuk 10 menit lagi', 7), ('me', 'Oke, saya tunggu di pintu masuk', 6)],
           'pub': [('anna', 'Halo semua! Ada yang mengudara dekat danau?', 40), ('me', 'Saya di sini, M9 dekat jembatan', 38), ('rita', 'Terdengar lewat North Hill', 30), ('anna', 'Sampai nanti malam!', 2)]},
}

NODES = [  # id, name, type (1 chat, 2 repeater, 3 room), key, lat, lon, snr, rssi, minutes ago, path length
    ('B0B5A11E7C42D9E3', 'Bob V4', 1, 'b0b5a11e', 46.0242, 7.7533, 9.25, -71, 1, 0),
    ('A22A4E1F03C5B7D8', 'Anna', 1, 'a22a4e1f', 46.0168, 7.7398, 6.5, -88, 2, 1),
    ('C0FFEE42D1B2A3F4', 'Rita T-Deck', 1, 'c0ffee42', None, None, 3.0, -97, 30, 2),
    ('A1D4C3B2E5F60718', 'North Hill Omni', 2, 'a1d4c3b2', 46.0301, 7.7620, 11.0, -64, 3, 0),
    ('11E0F1D2C3B4A596', 'River Tower', 2, '11e0f1d2', 46.0099, 7.7275, 4.75, -93, 12, 1),
    ('D4D4C0DEA1B2C3D4', 'Valley Room', 3, 'd4d4c0de', 46.0213, 7.7466, 7.5, -84, 45, 1),
]


def lang_of(query):
    code = query.get('lang', ['ru'])[0]
    return code if code in CHATS else 'en'


def messages(lang):
    out, n = [], 1
    names = {'bob': ('B0B5A11E7C42D9E3', 'Bob V4'), 'anna': ('A22A4E1F03C5B7D8', 'Anna'), 'rita': ('C0FFEE42D1B2A3F4', 'Rita T-Deck')}
    for chat, rows in CHATS[lang].items():
        for who, text, ago in rows:
            t = NOW - ago * 60
            mine = who == 'me'
            src, name = (ME, 'M9') if mine else names[who]
            dest = 'FFFFFFFFFFFFFFFF' if chat == 'pub' else ('B0B5A11E7C42D9E3' if mine else ME)
            m = {'protocol': 2, 'source': src, 'destination': dest, 'session': t, 'id': 1000 + n, 'name': name, 'text': text,
                 'time': t, 'outgoing': mine, 'status': 3 if mine else 0, 'route': 'direct' if chat == 'bob' else 'flood',
                 'hops': 0 if chat == 'bob' else 2, 'snr': 9.25, 'rssi': -71, 'heard': 1}
            if chat == 'pub' and not mine:
                m.update({'path_len': 2, 'path': 'A111', 'snr': 10.5, 'rssi': -88, 'heard': 3})
            if chat == 'pub' and mine:
                m.update({'status': 2, 'heard': 2, 'echoes': [{'hash': 'A1', 'snr': 11.0, 'rssi': -64}, {'hash': '11', 'snr': 4.75, 'rssi': -93}]})
            out.append(m)
            n += 1
    return sorted(out, key=lambda m: m['time'])


def nodes():
    return [{'public_key': key + '0' * 56, 'type': kind, 'heard': True, 'path_length': path, 'id': nid, 'name': name,
             'rssi': rssi, 'snr': snr, 'age_seconds': ago * 60, 'hops': path,
             'position': bool(lat), **({'latitude': lat, 'longitude': lon} if lat else {})}
            for nid, name, kind, key, lat, lon, snr, rssi, ago, path in NODES]


CHANNEL_COUNT = 3
STATUS = {
    'version': VERSION, 'board': 'm9', 'board_name': 'Elecrow ThinkNode M9', 'max_power': 22, 'family': 'm9', 'button': '',
    'absent': [], 'firmware': 'MeshMesh ' + VERSION, 'role': 'chat', 'node': ME, 'name': 'M9', 'network': 1,
    'build_sha256': '0' * 64, 'protocol': 'MeshCore', 'public_key': '5a7c21e0' + '0' * 56, 'channel': 'Public', 'channels': CHANNEL_COUNT,
    'public_message_limit': 133, 'unix_time': NOW, 'clock_source': 'phone', 'clock_age_seconds': 60, 'clock_conflict': False,
    'uptime': 15360, 'boot': 42, 'reset_reason': 1, 'heap': 180000, 'psram': 7800000, 'cpu_mhz': 80, 'history_count': 12,
    'power_on': 'button', 'power_button': 'PWR', 'radio': True, 'radio_error': 0, 'tx': 38, 'rx': 214, 'rejected': 0,
    'relayed': 0, 'contacts_saved': True, 'rssi': -71, 'snr': 9.25, 'keyboard': True, 'key_count': 0, 'last_key': 0,
    'battery_mv': 4020, 'sd': True, 'storage': True, 'rtc': True, 'rtc_valid': True, 'compass': True, 'compass_sample': True,
    'imu': True, 'imu_sample': True, 'gps_bytes': 120000, 'gps_sentences': 3000, 'gps_fix': True, 'satellites': 9,
    'latitude': HOME[0], 'longitude': HOME[1], 'mag': [0, 0, 0], 'accel': [0, 0, 0], 'clock_trusted': True, 'busy': False,
    'rx_age': 40, 'quick_send': True, 'people_counter': True, 'wardrive': True, 'notification_dropped': 0,
    'notification_led': True, 'event': '', 'wifi': True, 'wifi_radio': True, 'langs': ' '.join(['en', 'ru'] + LANGS[1:]),
    'modules': 'chess pet dice wardrive checkers sea', 'internet': False, 'ble': True, 'companion': True,
}
CONFIG = {
    'name': 'M9', 'frequency': 869.618, 'bandwidth': 62.5, 'sf': 8, 'cr': 8, 'power': 14, 'hops': 3, 'path_hash': 2,
    'relay': False, 'gps': True, 'sound': True, 'battery_volts': False, 'lang': 'en', 'russian': False, 'brightness': 180,
    'auto_lock': 120, 'dim_after': 30, 'lock_details': False, 'notify_led': 1, 'notify_wake': 1, 'notify_popup': 1,
    'notify_failed': True, 'notify_words': '', 'utc_offset': 120, 'region': '', 'net_auto': True,
    'apps': 'chats quick nodes nav map radar wardrive people {games chess checkers sea pet dice solitaire} connect health settings',
}
CHANNELS = {'max': 8, 'channels': [
    {'id': 'ALL', 'name': 'Public', 'kind': 'public', 'hash': '11', 'link': '', 'region': '',
     'policy': {'led': 3, 'wake': 3, 'popup': 3, 'priority': 1, 'device_limit': 0, 'app_limit': 0}},
    {'id': 'FF9C8B7A6D5E4F30', 'name': 'Friends', 'kind': 'private', 'hash': '3C', 'region': '',
     'link': 'meshcore://channel/add?name=Friends&secret=00112233445566778899aabbccddeeff',
     'policy': {'led': 3, 'wake': 3, 'popup': 3, 'priority': 1, 'device_limit': 0, 'app_limit': 0}},
    {'id': 'FF1A2B3C4D5E6F70', 'name': '#hiking', 'kind': 'hashtag', 'hash': '7A', 'link': '', 'region': '',
     'policy': {'led': 3, 'wake': 3, 'popup': 3, 'priority': 1, 'device_limit': 0, 'app_limit': 0}}],
    'heard': [], 'samples': 0, 'region': '', 'regions': {'state': 'idle', 'repeaters': 0, 'answered': 0, 'found': []}}
FEN = 'r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4'
GAME = {'id': '3F2A', 'peer': 'B0B5A11E7C42D9E3', 'name': 'Bob V4', 'color': 'white', 'plies': 6, 'state': 'playing',
        'my_turn': True, 'check': False, 'result': '', 'reason': '', 'draw_offer': '', 'out_status': 3, 'retry_in': -1,
        'retries': 0, 'auto_stopped': False, 'last_san': 'Nf6', 'last': 'g8f6', 'fen': FEN, 'rated': True, 'sign': '',
        'elo_before': 1500, 'elo_after': 1500, 'elo_counted': False, 'move_open': False, 'unseen': False, 'updated': NOW - 120}
# Checkers and sea battle (firmware "duel", src/Duel.cpp): positions played by the real code (tools/ui_preview).
DUEL_LIST = ('id', 'kind', 'peer', 'name', 'state', 'result', 'reason', 'my_turn', 'unseen', 'out_status', 'retry_in', 'auto_stopped',
             'updated', 'color', 'plies', 'last', 'draw_offer', 'mine_left', 'their_left', 'shots', 'first', 'aim', 'last_out', 'last_in',
             'ships_left', 'sunk', 'can_shuffle', 'fair')
DUELS = [
    {'id': '41A8', 'kind': 'checkers', 'peer': 'B0B5A11E7C42D9E3', 'name': 'Bob V4', 'state': 'playing', 'result': '', 'reason': '',
     'my_turn': True, 'unseen': False, 'out_status': 3, 'retry_in': -1, 'auto_stopped': False, 'updated': NOW - 300, 'color': 'white',
     'plies': 16, 'last': 'b6-c5', 'draw_offer': '', 'mine_left': 9, 'their_left': 9, 'board': '.wwwww.www...w...bbb...bb...bbbb',
     'legal': ['d4:f6:h4', 'd4:b6'], 'last_path': ['b6', 'c5']},
    {'id': '4BC8', 'kind': 'sea', 'peer': 'A22A4E1F03C5B7D8', 'name': 'Anna', 'state': 'playing', 'result': '', 'reason': '', 'my_turn': True,
     'unseen': False, 'out_status': 3, 'retry_in': -1, 'auto_stopped': False, 'updated': NOW - 600, 'shots': 28, 'first': True, 'aim': '',
     'last_out': 'c2', 'last_in': 'j8', 'ships_left': 10, 'sunk': 1, 'can_shuffle': False,
     'mine': '...o...s..s..s......s.oo.xxs......o..o....o.......ssss..ss...........o...s.s...o..os.o..s...os.ss...',
     'theirs': '...........ooo..............o...~~~x.o.o..~X~..o....~~~...o..x....o............o....x..............o'},
    {'id': '0D92', 'kind': 'checkers', 'peer': 'C0FFEE42D1B2A3F4', 'name': 'Rita T-Deck', 'state': 'invited', 'result': '', 'reason': '',
     'my_turn': False, 'unseen': True, 'out_status': 0, 'retry_in': -1, 'auto_stopped': False, 'updated': NOW - 60, 'color': 'white',
     'plies': 0, 'draw_offer': '', 'mine_left': 12, 'their_left': 12, 'board': 'wwwwwwwwwwww........bbbbbbbbbbbb', 'legal': []}]


def duel_list():
    games = [{k: g[k] for k in DUEL_LIST if k in g} for g in DUELS]
    return {'games': games, 'events': 0, 'event': '', 'kinds': ['checkers', 'sea'],
            'waiting': sum(1 for g in DUELS if g['state'] == 'invited' or g['my_turn'] or g['unseen'])}


def duel_static(line):
    """The fixed games: the player's answers change them, moves and shots need an opponent (--duel PROGRAM)."""
    if line == 'duel':
        return json.dumps(duel_list(), ensure_ascii=False)
    words = line.split()
    if len(words) >= 4 and words[1] == 'invite':
        peer, kind = words[2], words[3]
        names = {nid: name for nid, name, *_ in NODES}
        gid = '%04X' % random.randrange(0x1000, 0xFFFF)
        color = {'w': 'white', 'b': 'black'}.get(words[4] if len(words) > 4 else 'r', random.choice(['white', 'black']))
        g = {'id': gid, 'kind': kind, 'peer': peer, 'name': names.get(peer, peer), 'state': 'inviting', 'result': '', 'reason': '',
             'my_turn': False, 'unseen': False, 'out_status': 2, 'retry_in': -1, 'auto_stopped': False, 'updated': NOW}
        g.update({'color': color, 'plies': 0, 'draw_offer': '', 'mine_left': 12, 'their_left': 12, 'board': 'w' * 12 + '.' * 8 + 'b' * 12,
                  'legal': []} if kind == 'checkers' else {'shots': 0, 'first': True, 'aim': '', 'last_out': '', 'last_in': '',
                  'ships_left': 10, 'sunk': 0, 'can_shuffle': True, 'mine': SEA_FLEET, 'theirs': '.' * 100})
        DUELS.append(g)
        return f'OK game {gid} ' + (color if kind == 'checkers' else 'sea')
    g = next((x for x in DUELS if len(words) > 2 and x['id'] == words[2]), None)
    if not g:
        return 'ERR duel invite|accept|decline|move|shoot|shuffle|draw|resign|resend|remove|show|seen GAME_ID ...'
    verb = words[1]
    if verb == 'show':
        return json.dumps(g, ensure_ascii=False)
    if verb == 'seen':
        g['unseen'] = False
        return 'OK seen'
    if verb == 'accept' and g['state'] == 'invited':
        g.update(state='playing', my_turn=g.get('color') == 'white', unseen=False,
                 legal=['a3-b4', 'c3-b4', 'c3-d4', 'e3-d4', 'e3-f4', 'g3-f4', 'g3-h4'] if g.get('color') == 'white' else [])
    elif verb == 'decline' and g['state'] in ('invited', 'inviting'):
        g.update(state='over', reason='declined' if g['state'] == 'invited' else 'cancelled', my_turn=False, unseen=False)
    elif verb == 'resign' and g['state'] == 'playing':
        g.update(state='over', result='lost', reason='resigned', my_turn=False)
    elif verb == 'draw' and g['state'] == 'playing' and g['kind'] == 'checkers':
        g['draw_offer'] = 'mine'
    elif verb == 'remove' and g['state'] == 'over':
        DUELS.remove(g)
    elif verb == 'shuffle' and g.get('can_shuffle'):
        pass
    elif verb in ('move', 'shoot'):
        return 'ERR no opponent in this demo: tools/web_demo.py --duel PROGRAM plays against the real code'
    else:
        return f'ERR {verb} not possible now'
    return 'OK ' + verb


SEA_FLEET = DUELS[1]['mine'].translate(str.maketrans('xXo', 'ss.'))  # a new game's fleet: Anna's game before the shots


class DuelEngine:
    """Two copies of the real games code: "me" answers the page, "bob" plays against it on its own."""
    PEER = '1F00B2C3D4E5F600'  # the preview program's first chat contact: each copy sees the other as it
    BOB = ('B0B5A11E7C42D9E3', 'Bob V4')

    def __init__(self, program):
        self.lock = threading.Lock()
        self.me = subprocess.Popen([program, '--duel', 'en'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
        self.bob = subprocess.Popen([program, '--duel', 'en'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
        self.turn_seen = {}
        self.rng = random.Random()
        threading.Thread(target=self.loop, daemon=True).start()

    @staticmethod
    def ask(p, line):
        p.stdin.write(line + '\n')
        p.stdin.flush()
        return p.stdout.readline().strip()

    def outward(self, text):
        """Answers of "me": the preview's contact becomes Bob, the host's clock the demo's."""
        try:
            d = json.loads(text)
        except ValueError:
            return text
        now = time.time()
        for g in d.get('games', [d]) if isinstance(d, dict) else []:
            if g.get('peer') == self.PEER:
                g['peer'], g['name'] = self.BOB
            if g.get('updated'):
                g['updated'] = NOW - max(0, int(now - g['updated']))
        return json.dumps(d, ensure_ascii=False)

    def command(self, line):
        words = line.split()
        if len(words) > 2 and words[1] == 'invite':
            words[2] = self.PEER  # every invitation goes to Bob
            line = ' '.join(words)
        with self.lock:
            return self.outward(self.ask(self.me, line))

    def carry(self):
        n = 0
        for src, dst in ((self.me, self.bob), (self.bob, self.me)):
            for m in json.loads(self.ask(src, 'sent')):
                self.ask(dst, f"recv {self.PEER} {m['text']}")
                self.ask(src, f"ack {m['id']}")
                n += 1
        return n

    def play(self):
        """Bob: accepts, agrees to a draw, moves or shoots two seconds after his turn comes."""
        for g in json.loads(self.ask(self.bob, 'duel'))['games']:
            gid = g['id']
            if g['state'] == 'invited':
                self.ask(self.bob, f'duel accept {gid}')
            elif g['state'] == 'playing' and g.get('draw_offer') == 'theirs':
                self.ask(self.bob, f'duel draw {gid}')
            elif g['state'] == 'playing' and g['my_turn']:
                since = self.turn_seen.setdefault(gid, time.time())
                if time.time() - since < 2:
                    continue
                self.turn_seen.pop(gid, None)
                d = json.loads(self.ask(self.bob, f'duel show {gid}'))
                if g['kind'] == 'checkers' and d.get('legal'):
                    self.ask(self.bob, f"duel move {gid} {self.rng.choice(d['legal'])}")
                elif g['kind'] == 'sea':
                    cells = [c for c in range(100) if d['theirs'][c] == '.']
                    if cells:
                        c = self.rng.choice(cells)
                        self.ask(self.bob, f"duel shoot {gid} {'abcdefghij'[c % 10]}{c // 10 + 1}")
            elif g['state'] == 'over':
                self.ask(self.bob, f'duel remove {gid}')

    def loop(self):
        while True:
            time.sleep(1)
            with self.lock:
                for _ in range(10):
                    if not self.carry():
                        break
                self.play()
                self.carry()


DUEL = None  # DuelEngine with --duel PROGRAM


def duel_command(line):
    return DUEL.command(line) if DUEL else duel_static(line)


RADAR = {'active': True, 'wifi': 'scanning', 'ble': 'scanning', 'sweeps': 14, 'personal': 3, 'tracking': False, 'targets': [
    {'kind': 'lora', 'ref': 1, 'name': 'North Hill Omni', 'rssi': -64, 'age_ms': 3000, 'node': 'A1D4C3B2E5F60718'},
    {'kind': 'wifi', 'ref': 2, 'name': 'MM-21E0B4', 'rssi': -41, 'age_ms': 800, 'node': ''},
    {'kind': 'wifi', 'ref': 3, 'name': 'Cafe Guest', 'rssi': -58, 'age_ms': 1200, 'node': ''},
    {'kind': 'ble', 'ref': 4, 'name': 'Phone', 'rssi': -52, 'age_ms': 600, 'node': ''},
    {'kind': 'ble', 'ref': 5, 'name': 'Watch', 'rssi': -61, 'age_ms': 900, 'node': ''},
    {'kind': 'wifi', 'ref': 6, 'name': 'Library', 'rssi': -66, 'age_ms': 1500, 'node': ''},
    {'kind': 'lora', 'ref': 7, 'name': 'Bob V4', 'rssi': -71, 'age_ms': 5000, 'node': 'B0B5A11E7C42D9E3'}],
    'csi': {'role': 'off', 'running': False, 'rate': 0},
    'focus': {'kind': 'wifi', 'name': 'Cafe Guest', 'channel': 6, 'node': '', 'device': '', 'vendor': '', 'rssi': -52, 'fast': -52.4,
              'peak': -49, 'fresh': True, 'samples': 9, 'rate': 10, 'sample_age_ms': 200, 'trend': 1,
              'history': [-71, -70, -70, -69, -68, -68, -67, -66, -66, -65, -64, -63, -63, -62, -61, -60, -60, -59, -58, -58, -57, -56, -56, -55, -54, -54, -53, -52, -52, -52]}}
CSI = {'role': 'sensor', 'running': True, 'rate': 48, 'heard': True, 'rssi': -44, 'stale': False, 'calibrated': True,
       'calibrating_ms': None, 'activity': 7.1, 'threshold': 2.4, 'moving': True,
       'history': [0.4, 0.5, 0.4, 0.6, 0.5, 0.4, 0.5, 0.7, 0.6, 0.5, 0.4, 0.5, 0.6, 0.8, 1.1, 1.9, 3.2, 4.4, 5.6, 6.3, 6.9, 7.4, 7.1, 6.8, 7.0, 7.3, 7.1]}


def mmt(png, query):
    """An OSM tile as the board keeps it (docs/maps-format.md): MMT1 header, runs of RGB565 pixels."""
    import struct
    import zlib
    from PIL import Image
    pixels = Image.open(png).convert('RGB').getdata()
    runs, last, n = bytearray(), None, 0
    for r, g, b in pixels:
        p = (r >> 3) << 11 | (g >> 2) << 5 | b >> 3
        if p == last and n < 65535:
            n += 1
            continue
        if n:
            runs += struct.pack('<HH', n, last)
        last, n = p, 1
    runs += struct.pack('<HH', n, last)
    z, x, y = (int(query[k][0]) for k in 'zxy')
    return b'MMT1' + bytes([z, 0, 0, 0]) + struct.pack('<IIII', x, y, len(runs), zlib.crc32(runs)) + bytes(runs)


# The radio profiles of firmware 0.27.0 (src/App.cpp radioCommand): the previous profile, a test not saved.
RADIO = ('frequency', 'bandwidth', 'sf', 'cr')


def radio_set(values, mode='normal'):
    now = {k: CONFIG[k] for k in RADIO}
    nxt = {**now, **{k: values[k] for k in RADIO if k in values}}
    if not 863 <= nxt['frequency'] <= 870:
        return 'ERR invalid settings; 868 MHz range is 863..870'
    CONFIG.update({k: v for k, v in values.items() if k not in RADIO})
    if nxt != now:
        trial = CONFIG.get('radio_trial')
        if mode == 'try':
            if not trial:
                CONFIG['radio_trial'] = now
            if CONFIG['radio_trial'] == nxt:
                CONFIG.pop('radio_trial')
        elif mode == 'revert':
            CONFIG.pop('radio_trial', None)
        elif trial:
            CONFIG['radio_prev'] = CONFIG.pop('radio_trial')
        else:
            CONFIG['radio_prev'] = now
        CONFIG.update(nxt)
    return 'OK settings saved'


def radio_command(line):
    if line.startswith('radio try '):
        return radio_set(json.loads(line[10:]), 'try')
    if line == 'radio keep':
        if 'radio_trial' not in CONFIG:
            return 'ERR radio: no test'
        CONFIG['radio_prev'] = CONFIG.pop('radio_trial')
        return 'OK radio test saved'
    if line == 'radio revert':
        return radio_set(CONFIG['radio_trial'], 'revert') if 'radio_trial' in CONFIG else 'ERR radio: no test'
    if line == 'radio previous':
        return radio_set(CONFIG['radio_prev']) if 'radio_prev' in CONFIG else 'ERR radio: no previous settings'
    return 'ERR radio try {JSON}|keep|revert|previous'


# The network search of the board (netfind): a finished one, the Moscow profile answered by three repeaters.
NETFIND = {'state': 'done', 'auto': False, 'located': True, 'step': 3, 'total': 3, 'best': 1, 'found': 0, 'enabled': True,
           'catalogue': '2026-10-29', 'results': [
               {'frequency': 869.618, 'bandwidth': 62.5, 'sf': 8, 'cr': 8, 'current': True, 'done': True, 'repeaters': 0, 'packets': 0},
               {'frequency': 868.731, 'bandwidth': 62.5, 'sf': 7, 'cr': 7, 'place': 'Москва и Московская область', 'done': True,
                'repeaters': 3, 'packets': 4, 'snr': 6.5},
               {'frequency': 868.731, 'bandwidth': 62.5, 'sf': 8, 'cr': 8, 'place': 'Москва и Московская область', 'done': True,
                'repeaters': 0, 'packets': 0}]}


def netfind_command(line):
    if line == 'netfind':
        return json.dumps(NETFIND, ensure_ascii=False)
    if line == 'netfind stop' or line.startswith('netfind start'):
        return 'OK network search started' if line.startswith('netfind start') else 'OK network search stopped'
    if line.startswith('netfind use '):
        r = NETFIND['results'][int(line[12:])]
        return radio_set({k: r[k] for k in ('frequency', 'bandwidth', 'sf', 'cr')}, 'try')
    return 'ERR netfind start [all|LAT LON]|stop|use N'


DEMO_KEY = '5a7c' + '00' * 62  # a demo node key (not a real one): "identity" for the backup page (docs/backup.md)


def backup_command(line):
    """The commands of the backup page that the demo answers: reads as the board does, writes accepted."""
    reads = {'config': '/api/config', 'channels': '/api/channels', 'nodes': '/api/nodes', 'quick': '/api/quick',
             'people': '/api/people', 'dice': '/api/dice'}
    if line in reads:
        return json.dumps(answer(reads[line], {}), ensure_ascii=False)
    if line == 'identity':
        return json.dumps({'private_key': DEMO_KEY, 'public_key': STATUS.get('public_key', '')})
    if line == 'dice sheet export':
        return json.dumps({'text': 'MMS1~General|', 'bytes': 13, 'parts': 1})
    if line.startswith('dice '):
        return 'OK ' + json.dumps(answer('/api/dice', {}))
    if line.startswith('channel do ') and '"add"' in line:
        return 'OK channel added 0000C0FFEE01'
    if line.startswith('identity set '):
        return 'OK identity unchanged'
    if line == 'fsformat':
        if STATUS['storage']:
            return 'ERR filesystem already mounted; no format'
        STATUS.update(storage=True, radio=True)
        return 'OK MeshMesh filesystem initialized'
    if line.startswith('set ') and not STATUS['storage']:
        return 'ERR settings storage; restored previous'
    if line.startswith('set '):
        return radio_set(json.loads(line[4:]))
    return 'OK'


def answer(path, query):
    lang = lang_of(query)
    if path == '/api/status':
        return STATUS
    if path == '/api/messages':
        return messages(lang)
    if path == '/api/nodes':
        return nodes()
    if path == '/api/config':
        return {**CONFIG, 'lang': lang if lang != 'ru' else 'ru', 'russian': lang == 'ru'}
    if path == '/api/navigation':
        return {'available': True, 'calibrated': True, 'calibrating': False, 'samples': 0, 'heading_valid': True, 'magnetic_heading': 37, 'note': ''}
    if path == '/api/maps':
        return {'available': True, 'tiles': 15, 'name': 'Zermatt', 'latitude': HOME[0], 'longitude': HOME[1], 'center': True, 'zoom': 15,
                'follow': True, 'uploading': False, 'received': 0, 'expected': 0, 'error': '', 'compressed_upload': True,
                'online': False, 'fetching': 0, 'located': False, 'cached': 0}
    if path == '/api/maps/areas':
        return [{'id': 1, 'name': 'Zermatt', 'tiles': 15, 'bytes': 368000}]
    if path == '/api/chess':
        if 'id' in query:
            return {'id': GAME['id'], 'san': ['e4', 'e5', 'Nf3', 'Nc6', 'Bc4', 'Nf6'], 'legal': ['f3g5', 'b1c3', 'd2d3', 'e1g1'], 'check': False}
        return {'events': 1, 'event': '', 'waiting': 1, 'elo': 1512, 'tours': 0, 'sd': True, 'games': [GAME]}
    if path == '/api/tour':
        return []
    if path == '/api/duel':
        r = duel_command(f"duel show {query['id'][0]}" if 'id' in query else 'duel')
        return None if r.startswith('ERR') else json.loads(r)
    if path == '/api/channels':
        return CHANNELS
    if path == '/api/radar':
        return {**RADAR, 'csi': CSI} if 'csi' in query else {**RADAR, 'tracking': True} if 'homing' in query else RADAR
    if path == '/api/connections':
        return {'wifi': True, 'ssid': 'MM-21E0B4', 'password': 'DEMO-PASSWORD', 'ip': '192.168.4.1', 'ble': True,
                'ble_name': 'MeshCore-M9 5A7C', 'pin': 123456}
    if path == '/api/pet':
        return {'stage': 'adult', 'generation': 1, 'mortal': True, 'graves': [], 'name': 'Ping', 'mood': 'happy', 'level': 3, 'xp': 40,
                'next_level_xp': 100, 'food': 82, 'joy': 90, 'health': 100, 'snacks': 2, 'age_s': 400000, 'hatch_left_s': 0,
                'species': 0, 'kind': 'blob', 'packets': 214, 'relays': 0, 'messages': 12, 'acks': 8, 'friends': 3, 'wins': 1,
                'cuddles': 5, 'walks': 2, 'sprite': '', 'body': '', 'accent': '', 'needs_care': False}
    if path == '/api/dice':
        return {'mode': 'rpg', 'character': 0, 'characters': ['General'], 'pool': {'count': 2, 'type': 'd6', 'mod': 1, 'text': '2d6+1'},
                'rolls': [], 'counters': [], 'results': [], 'rolls_total': 0,
                'grid': {'type': 'd6', 'size': 20, 'chosen': 0, 'rolled': False, 'threshold': 4, 'threshold_on': False, 'successes': 0, 'values': [1] * 20}}
    if path == '/api/quick':
        return {'to': 'ALL', 'name': 'Public', 'valid': True, 'gps': True, 'limit': 133, 'presets': ['OK', '👍', '', '', '', '', '', '', '', '']}
    if path == '/api/people':
        return {'manual': 27, 'current': 0, 'counters_max': 8, 'counters': [{'name': '', 'value': 27, 'color': 1}, {'name': '', 'value': 4, 'color': 7}, {'name': '', 'value': 12, 'color': 5}],
                'ble': True, 'wifi': False, 'personal_only': False, 'window': 60, 'rssi': -85, 'ble_devices': 7, 'wifi_devices': 0,
                'ble_saturated': False, 'wifi_saturated': False, 'ble_dropped': 0, 'wifi_dropped': 0, 'ble_state': 'scanning',
                'wifi_state': 'off', 'capacity': 128, 'wifi_supported': True}
    if path == '/api/wardrive':
        return {'supported': True, 'passive': True, 'ping': False, 'coords': False, 'nets': False, 'nets_supported': True, 'distance': 200,
                'interval': 60, 'position': {'lat': HOME[0], 'lon': HOME[1], 'source': 'gps'}, 'gps_fix': True, 'clock': True, 'points': 37,
                'nets_logged': 0, 'capacity': 512, 'nets_capacity': 2048, 'lost': 0, 'no_fix': 0, 'pings': 0, 'pings_heard': 0,
                'pings_failed': 0, 'pinging': False, 'repeaters': []}
    return None


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def send(self, code, body, kind):
        data = body if isinstance(body, bytes) else body.encode()
        self.send_response(code)
        self.send_header('Content-Type', kind)
        self.send_header('Content-Length', str(len(data)))
        self.send_header('Cache-Control', 'no-store')
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        url = urlparse(self.path)
        query = parse_qs(url.query)
        if url.path == '/':
            page = (ROOT / 'web/index.html').read_text()
            # Signed in already: the page keeps a login across a reload of the language this way.
            page = page.replace('<script id="i18n">', "<script>sessionStorage.setItem('mm-auth','Basic demo');"
                                "const mmErr=m=>{document.documentElement.dataset.errors=(document.documentElement.dataset.errors||'')+m+' | '};"
                                "addEventListener('error',e=>mmErr(e.message));addEventListener('unhandledrejection',e=>mmErr(String(e.reason&&e.reason.stack||e.reason)))"
                                "</script><script id=\"i18n\">", 1)
            if 'run' in query:  # ?run=JS: done once the page shows its data (a tab, a selection)
                page = page.replace('</body>', f"<script>setTimeout(()=>{{try{{{query['run'][0]}}}catch(e){{mmErr(e.stack)}}}},2500)</script></body>", 1)
            if 'probe' in query:  # ?probe=renderHome: the error of one page function, for looking into the demo
                fn = query['probe'][0]
                page = page.replace('</body>', f"<script>setTimeout(()=>{{try{{{fn}()}}catch(e){{mmErr(e.stack)}}}},3000)</script></body>", 1)
            return self.send(200, page, 'text/html; charset=utf-8')
        if url.path.startswith('/i18n/') and url.path.endswith('.js'):
            f = ROOT / 'i18n/web' / (url.path[6:-3] + '.json')
            if not f.exists():
                return self.send(404, 'no', 'text/plain')
            d = {k: v for k, v in json.loads(f.read_text()).items() if (v if isinstance(v, str) else any(v.values()))}
            return self.send(200, 'globalThis.MeshMeshWebDict=' + json.dumps(d, ensure_ascii=False) + ';', 'application/javascript; charset=utf-8')
        if url.path == '/networks.json':  # the catalogue the app carries next to the page
            return self.send(200, (ROOT / 'site/networks.json').read_text(), 'application/json; charset=utf-8')
        if url.path == '/api/maps/tile':
            f = ROOT / 'tools/web_demo/tiles' / f"{query.get('z', [''])[0]}_{query.get('x', [''])[0]}_{query.get('y', [''])[0]}.png"
            return self.send(200, mmt(f, query), 'application/octet-stream') if f.exists() else self.send(404, 'Map tile not saved', 'text/plain')
        if url.path.startswith('/api/'):
            ref = parse_qs(urlparse(self.headers.get('Referer', '')).query)
            data = answer(url.path, {**ref, **query})
            if data is None:
                return self.send(404, 'Not found', 'text/plain')
            return self.send(200, json.dumps(data, ensure_ascii=False), 'application/json')
        return self.send(404, 'Not found', 'text/plain')

    def do_POST(self):
        url = urlparse(self.path)
        try:
            body = json.loads(self.rfile.read(int(self.headers.get('Content-Length') or 0)) or b'null')
        except ValueError:
            body = None
        reply = 'OK'
        if url.path == '/api/config' and isinstance(body, dict):
            reply = radio_set(body) if STATUS['storage'] else 'ERR settings storage; restored previous'
        elif url.path == '/api/command' and isinstance(body, dict) and str(body.get('command', '')).startswith('radio '):
            reply = radio_command(body['command'])
        elif url.path == '/api/command' and isinstance(body, dict) and str(body.get('command', '')).startswith('netfind'):
            reply = netfind_command(body['command'])
        elif url.path == '/api/command' and isinstance(body, dict) and str(body.get('command', '')).startswith('duel'):
            reply = duel_command(body['command'])
        elif url.path == '/api/command' and isinstance(body, dict):
            reply = backup_command(str(body.get('command', '')))
        self.send(400 if reply.startswith('ERR') else 200, reply, 'text/plain')


def serve(port):
    server = ThreadingHTTPServer(('127.0.0.1', port), Handler)
    return server


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument('--port', type=int, default=8790)
    p.add_argument('--no-storage', action='store_true', help='a board after Meshtastic: no storage, sending off, until fsformat')
    p.add_argument('--duel', metavar='PROGRAM', help='checkers and sea battle by the real code against a scripted Bob (tools/ui_preview/build.sh KEEP=PROGRAM)')
    a = p.parse_args()
    if a.duel:
        DUEL = DuelEngine(a.duel)
    if a.no_storage:
        STATUS.update(storage=False, radio=False)
    print(f'http://127.0.0.1:{a.port}/?lang=en#home')
    serve(a.port).serve_forever()
