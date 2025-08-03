FROM --platform=linux/arm64 debian:bookworm

# 작업 디렉토리 설정 
WORKDIR /app

# 패키지 목록 업데이트 및 필수 패키지 설치
RUN apt update && apt install -y \
    build-essential \
    cmake \
    pkg-config \
    libgstreamer1.0-dev \
    libgstrtspserver-1.0-dev \
    libsqlitecpp-dev \
    libsodium-dev \
    libcurl4-openssl-dev \
    libssl-dev \
    sqlite3 \
    && rm -rf /var/lib/apt/lists/*

# 컨테이너 실행 후 bash 쉘 실행
CMD ["/bin/bash"]

# 도커 이미지 빌드
# sudo docker buildx build --platform linux/arm64 -t raspi-server . --load

# 컨테이너 실행
# sudo docker run -it --platform linux/arm64 --name raspi-server -v "$(pwd):/app" raspi-server

# 컨테이너 재접속
# sudo docker start raspi-server && sudo docker exec -it raspi-server /bin/bash