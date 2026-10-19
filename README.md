# JC2432W328C 서울 시계

PlatformIO, Arduino, LovyanGFX로 만든 JC2432W328C용 가로 화면 시계입니다.
인터넷의 NTP 서버에서 시간을 받아 한국 시간(KST, UTC+9)과 날짜를 표시합니다.

## Wi-Fi 설정

부팅하면 NVS에 저장된 Wi-Fi로 12초 동안 자동 연결을 시도합니다. 연결되지 않으면 화면에
주변 2.4 GHz Wi-Fi 목록이 열립니다.

1. 목록에서 AP를 터치합니다.
2. 보안 AP라면 화면 키보드로 비밀번호를 입력합니다.
3. `Connect`를 누릅니다.

키보드는 입력한 비밀번호를 화면에 표시합니다. `Shift`로 대·소문자를 바꾸고 `?123`으로
기호 자판을 엽니다. `Back`은 한 글자를
지우며, 입력 내용이 없을 때 누르면 AP 목록으로 돌아갑니다. 비밀번호는 최대 63자입니다.
연결에 성공한 SSID와 비밀번호는 ESP32의 NVS에 저장되어 다음 부팅 때 자동으로 사용됩니다.
시계 화면 오른쪽 아래의 `Wi-Fi` 버튼을 누르면 언제든 네트워크를 다시 선택할 수 있습니다.

## 외부 자동 업데이트

보드는 인터넷 연결 후 약 30초 뒤, 이후 6시간마다 GitHub Release의 `version.json`을
확인합니다. 새 버전이면 HTTPS로 `firmware.bin`을 받고 SHA-256을 검증한 다음 OTA 파티션을
전환하여 재부팅합니다.

배포 순서는 다음과 같습니다.

1. `include/app_config.h`의 `APP_VERSION`을 올립니다.
2. `build.cmd`로 빌드합니다.
3. `prepare-release.cmd wjdworud111-sudo/esp32_project 버전`을 실행합니다.
4. 생성된 `dist/firmware.bin`과 `dist/version.json`을 같은 GitHub Release에 첨부합니다.

`version.json`과 `firmware.bin`의 파일명은 바꾸지 않아야 합니다. 최초 OTA 지원 펌웨어는
USB로 한 번 업로드해야 하며, 이후부터 외부 네트워크에서 자동 업데이트할 수 있습니다.

### GitHub 자동 게시

`.github/workflows/firmware-release.yml`이 `main` 브랜치의 펌웨어 파일 변경을 감지합니다.
GitHub Actions가 PlatformIO 빌드, SHA-256 매니페스트 생성, 최신 Release 게시를 자동으로
수행합니다. 새 펌웨어를 배포할 때는 반드시 `include/app_config.h`의 `APP_VERSION`을 먼저
올려야 합니다. 같은 버전의 Release가 이미 있으면 기존 배포를 덮어쓰지 않고 작업이
실패합니다.

## 빌드

프로젝트 전용 PlatformIO 환경이 포함되어 있습니다.

```powershell
.\build.cmd
```

펌웨어는 `.pio/build/jc2432w328c/firmware.bin`에 생성됩니다.

## 확인된 하드웨어 배선

LCD와 터치 핀은 아래 JC2432W328C 공개 예제를 기준으로 확인했습니다.

- [LCD 예제](https://github.com/pay191/DIY_Malls-JC2432W328C_Tests/tree/main/BigYellowDisplay_2_ScreenTest)
- [CST820 터치 예제](https://github.com/pay191/DIY_Malls-JC2432W328C_Tests/tree/main/BigYellowDisplay_3_CapcitiveTouch)

| 기능 | GPIO |
|---|---:|
| LCD SCLK | 14 |
| LCD MOSI | 13 |
| LCD DC | 2 |
| LCD CS | 15 |
| LCD 백라이트 | 27 |
| LCD RST / MISO | 사용 안 함 |
| CST820 SDA | 33 |
| CST820 SCL | 32 |
| CST820 RST | 25 |
| CST820 INT | 21 |

연결된 칩은 업로드 로그에서 ESP32-D0WD-V3 revision 3.1로 확인했고, 사용자가 LCD 출력을
확인했습니다. CST820 칩 ID의 기대값은 `0xB7`이며 부팅 때 시리얼 로그에 실제 값을 표시합니다.
