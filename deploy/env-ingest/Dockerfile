FROM python:3.11-slim

WORKDIR /app

RUN pip install --no-cache-dir paho-mqtt asyncpg

COPY main.py /app/main.py

CMD ["python", "main.py"]
