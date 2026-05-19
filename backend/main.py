from flask import Flask, request, jsonify
from flask_cors import CORS
from datetime import datetime
import sqlite3
import os

app = Flask(__name__)
CORS(app)

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DB_FILE = os.path.join(BASE_DIR, 'database.db')

def init_db():
    conn = sqlite3.connect(DB_FILE)
    cursor = conn.cursor()
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT,
            pm25 REAL,
            temperature REAL,
            humidity REAL,
            status TEXT,
            advice TEXT
        )
    ''')
    conn.commit()
    conn.close()

init_db()

def analyze_air(pm, temp, hum):
    if temp >= 45 or (temp >= 38 and pm > 50):
        return {"status": "🔴 ЙМОВІРНА ПОЖЕЖА!", "advice": "Негайно перевірте приміщення! Можливе загоряння."}
    elif pm > 40 and hum >= 65:
        return {"status": "💨 Палять вейп (Пара)", "advice": "Зафіксовано електронні сигарети. Відчиніть вікно."}
    elif pm > 80 and hum < 65:
        return {"status": "🚬 Курять сигарети", "advice": "Сильний тютюновий дим! Терміново увімкніть витяжку."}
    elif pm > 35.5:
        return {"status": "🟣 Забруднення пилом", "advice": "Повітря брудне. Рекомендується вологе прибирання."}
    elif pm <= 35.5 and temp > 27 and hum > 60:
        return {"status": "🟡 Душно (Погана вентиляція)", "advice": "Пилу майже немає, але в кабінеті парко."}
    elif pm > 15.0:
        return {"status": "🟡 Помірна якість", "advice": "Нормальне повітря, але варто уникати застою."}
    else:
        return {"status": "🟢 Повітря чисте", "advice": "Ідеальний мікроклімат! Жодних дій не потрібно."}

@app.route('/api/data', methods=['POST', 'GET'])
def handle_data():
    if request.method == 'POST':
        data = request.json
        pm = data.get('pm25', 0)
        temp = data.get('temperature', 0)
        hum = data.get('humidity', 0)

        analysis = analyze_air(pm, temp, hum)
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

        conn = sqlite3.connect(DB_FILE)
        cursor = conn.cursor()
        cursor.execute('''
            INSERT INTO history (timestamp, pm25, temperature, humidity, status, advice)
            VALUES (?, ?, ?, ?, ?, ?)
        ''', (timestamp, pm, temp, hum, analysis["status"], analysis["advice"]))
        conn.commit()
        conn.close()

        return jsonify({"message": "Дані успішно збережено в БД SQLite", "analysis": analysis})

    elif request.method == 'GET':
        conn = sqlite3.connect(DB_FILE)
        conn.row_factory = sqlite3.Row
        cursor = conn.cursor()

        cursor.execute('''
            SELECT * FROM (
                SELECT * FROM history ORDER BY id DESC LIMIT 100
            ) ORDER BY id ASC
        ''')
        rows = cursor.fetchall()
        conn.close()

        history_data = []
        for row in rows:
            history_data.append({
                "timestamp": row["timestamp"],
                "pm25": row["pm25"],
                "temperature": row["temperature"],
                "humidity": row["humidity"],
                "status": row["status"],
                "advice": row["advice"]
            })

        return jsonify({
            "total_records": len(history_data),
            "history": history_data
        })