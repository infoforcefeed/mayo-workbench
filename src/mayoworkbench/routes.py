from flask import Blueprint

app = Blueprint('mayoworkbench', __name__, template_folder='templates')


@app.route('/')
def index():
    return 'Hello, Mayo'
