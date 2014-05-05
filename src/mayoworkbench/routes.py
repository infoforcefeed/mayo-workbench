from flask import Blueprint


app = Blueprint('mayoworkbench', __name__, templates_folder='templates')


@app.route('/')
def index():
    return 'Hello, Mayo'
