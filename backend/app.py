from flask import Flask

from login import login_bp


def create_app() -> Flask:
  app = Flask(__name__)
  app.register_blueprint(login_bp)

  @app.get('/health')
  def health():
    return {'status': 'ok'}

  return app


app = create_app()


if __name__ == '__main__':
  app.run(debug=True, port=5001)
