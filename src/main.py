from flask import Flask

from mayoworkbench.routes import app as routes

app = Flask('mayoworkbench', __name__, static_folder='mayoworkbench/static')
app.register_blueprint(routes)


def make_app(app):
    return app


if __name__ == '__main__':
    app = make_app(app)
    app.debug = True
    app.run()
