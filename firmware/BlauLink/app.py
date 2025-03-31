from flask import Flask, jsonify, render_template, send_from_directory
import os

app = Flask(__name__, template_folder='data', static_folder='data')  # Serveix també la carpeta "data" com a carpeta per arxius estàtics

@app.route('/')
def index():
    return render_template('wifimanager.html')  # Flask buscarà 'wifimanager.html' a la carpeta "data"


# Ruta per servir el fitxer CSS
@app.route("/style.css")
def style():
    return send_from_directory('data', 'style.css')

@app.route('/mac')
def get_mac():
    mac = '66:66:66:66:66:66'

    return mac

@app.route('/macList')
def get_mac_list():
    macList = [
        '64:e8:33:c7:62:50 >> Potato',
        '34:e8:33:c7:62:51 >> Banana',
        '24:e8:33:c7:62:52 >> Croassant'
    ]
    return "\n".join(macList)


if __name__ == '__main__':
    app.run(debug=True)
