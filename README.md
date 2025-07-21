Linux Raspberry Pi

필요 패키지 설치 명령어 :
sudo apt update
sudo apt install libgstreamer1.0-dev libgstrtspserver-1.0-dev
sudo apt install libcurl4-openssl-dev

(Optional) 부가 패키지 설치 명령어 :
sudo apt install sqlite3 // sqlite db 수동 조작용

OpenSSL용 서버 키 생성 가이드 :
1. https://wldh0026.atlassian.net/wiki/spaces/C/pages/9666620/cert
2. https://wldh0026.atlassian.net/wiki/spaces/C/pages/9863170/cert2

빌드 및 실행
```
make
```
이후 생성된 .sh 스크립트 파일 실행하여 코드 동작
