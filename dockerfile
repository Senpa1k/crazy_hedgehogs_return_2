FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    g++ \
    pkg-config \
    libpqxx-dev \
    libpq-dev \
    libasio-dev \
    wget \
    build-essential

WORKDIR /app

RUN wget https://github.com/CrowCpp/Crow/releases/download/v1.2.0/crow_all.h -O /usr/local/include/crow.h

COPY main.cc ./
COPY front ./public

RUN g++ -std=c++17 -fPIC main.cc -o phonebook_server -lpqxx -lpq -lpthread $(pkg-config --cflags --libs libpqxx)

EXPOSE 8080

CMD ["./phonebook_server"]