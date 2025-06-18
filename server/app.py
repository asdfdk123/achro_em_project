from flask import Flask, render_template
import mysql.connector

app = Flask(__name__)

@app.route('/')
def show_ranking():
    conn = mysql.connector.connect(
        host='localhost',
        user='game_user',
        password='1234',
        database='rhythm_game'
    )
    cursor = conn.cursor()
    cursor.execute("SELECT score FROM game ORDER BY score DESC LIMIT 10")
    rankings = cursor.fetchall()
    conn.close()
    return render_template('ranking.html', rankings=rankings)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
