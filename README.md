# Steam 연동 가이드

## 목차
1. [사전 준비](#1-사전-준비)
2. [Steamworks.NET 설치](#2-steamworksnet-설치)
3. [Steam 초기화 코드](#3-steam-초기화-코드)
4. [로그인 및 유저 정보](#4-로그인-및-유저-정보)
5. [서버 인증 (Auth Ticket)](#5-서버-인증-auth-ticket)
6. [서버 측 검증](#6-서버-측-검증)
7. [테스트 방법](#7-테스트-방법)
8. [빌드 및 배포](#8-빌드-및-배포)
9. [완성 코드](#9-완성-코드)

---

## 1. 사전 준비

### App ID란?
| 종류 | App ID | 설명 |
|------|--------|------|
| 테스트용 | 480 (Spacewar) | Valve가 개발자용으로 공개한 무료 테스트 ID. 누구나 사용 가능 |
| **Deadly Trick** | **3088400** | 실제 배포용 App ID |

### 필요한 계정 및 비용
1. **Steam 개발자 계정**: https://partner.steamgames.com 에서 가입
2. **앱 등록비**: $100 USD (1회)
3. **Web API Key**: Steamworks 파트너 사이트에서 무료 발급

### Web API Key란?
서버에서 Steam Web API를 호출할 때 필요한 인증 키.

**왜 필요한가?**
```
클라이언트 → Auth Ticket 발급 → 서버로 전송
서버 → Steam Web API로 검증 요청 (Key 필요!)
```
서버가 Steam에 "이 티켓 진짜야?" 물어볼 때 Key가 있어야 Steam이 응답해줌.

| 상황 | Web API Key |
|------|-------------|
| 싱글플레이 게임 | 필요 없음 |
| 멀티플레이 + 클라만 체크 | 필요 없음 (보안 취약) |
| 멀티플레이 + 서버 검증 | **필요** |

> 클라이언트에서 `SteamUser.BLoggedOn()`, `SteamApps.BIsSubscribed()` 호출은 Key 불필요.
> 서버에서 유저 인증을 검증하려면 필수. 클라이언트 체크만 하면 해킹으로 우회 가능.

### Web API Key 발급 방법
1. https://partner.steamgames.com 로그인
2. Users & Permissions → Manage Groups
3. 그룹 선택 → Web API Key 생성

---

## 2. Steamworks.NET 설치

### 방법 1: Unity Package Manager (권장)
1. Window → Package Manager 열기
2. + 버튼 → "Add package from git URL"
3. 아래 URL 입력:
```
https://github.com/rlabrecque/Steamworks.NET.git?path=/com.rlabrecque.steamworks.net
```

### 방법 2: UnityPackage 다운로드
1. https://github.com/rlabrecque/Steamworks.NET/releases 접속
2. 최신 `.unitypackage` 다운로드
3. Unity에서 Assets → Import Package → Custom Package

### steam_appid.txt 생성
프로젝트 루트 폴더에 `steam_appid.txt` 파일 생성:
```
3088400
```
> **주의**: 이 파일은 에디터/개발용. 실제 빌드에는 포함하지 않음

---

## 3. Steam 초기화 코드

### SteamManager.cs
`Assets/@Scripts/Managers/` 폴더에 생성:

```csharp
using Steamworks;
using UnityEngine;

public class SteamManager : MonoBehaviour
{
    public static SteamManager Instance { get; private set; }

    public bool IsInitialized { get; private set; }
    public CSteamID SteamID { get; private set; }
    public string PlayerName { get; private set; }

    void Awake()
    {
        // 싱글톤
        if (Instance != null)
        {
            Destroy(gameObject);
            return;
        }
        Instance = this;
        DontDestroyOnLoad(gameObject);

        InitializeSteam();
    }

    void InitializeSteam()
    {
        // Steam 클라이언트 실행 체크
        if (SteamAPI.RestartAppIfNecessary(new AppId_t(3088400)))
        {
            Debug.Log("Steam 클라이언트를 통해 재시작 필요");
            Application.Quit();
            return;
        }

        try
        {
            IsInitialized = SteamAPI.Init();

            if (!IsInitialized)
            {
                Debug.LogError("[Steam] 초기화 실패! Steam 클라이언트가 실행 중인지 확인하세요.");
                return;
            }

            // 유저 정보 캐싱
            SteamID = SteamUser.GetSteamID();
            PlayerName = SteamFriends.GetPersonaName();

            Debug.Log($"[Steam] 초기화 성공! 유저: {PlayerName} ({SteamID})");
        }
        catch (System.DllNotFoundException e)
        {
            Debug.LogError($"[Steam] DLL 로드 실패: {e.Message}");
            Debug.LogError("steam_api64.dll이 빌드 폴더에 있는지 확인하세요.");
        }
    }

    void Update()
    {
        if (IsInitialized)
        {
            SteamAPI.RunCallbacks(); // Steam 이벤트 처리 (필수!)
        }
    }

    void OnApplicationQuit()
    {
        if (IsInitialized)
        {
            SteamAPI.Shutdown();
        }
    }
}
```

---

## 4. 로그인 및 유저 정보

### 로그인 상태 확인
```csharp
// Steam에 로그인되어 있는지 확인
public bool IsLoggedIn()
{
    return IsInitialized && SteamUser.BLoggedOn();
}
```

### 구매(소유) 여부 확인
Steam 초기화만으로는 구매 여부를 자동 체크하지 않음. 직접 확인 필요:

```csharp
// 현재 앱을 소유하고 있는지 (구매했는지)
public bool OwnsGame()
{
    if (!IsInitialized) return false;
    return SteamApps.BIsSubscribed();
}

// 특정 App ID로 체크 (DLC 등 확인할 때 유용)
public bool OwnsApp(uint appId)
{
    if (!IsInitialized) return false;
    return SteamApps.BIsSubscribedToApp(new AppId_t(appId));
}

// 게임 시작 시 체크 예시
void Start()
{
    if (!OwnsGame())
    {
        Debug.LogError("[Steam] 게임을 소유하고 있지 않음!");
        // 스토어 페이지로 이동하거나 종료
        Application.Quit();
        return;
    }
}
```

> **참고**: 테스터(Manage Testers에 추가된 계정)나 키 등록 계정도 소유자로 인식됨

### 로그인 유지 체크 (연결 끊김 감지)
`SteamAPI.RunCallbacks()`가 돌고 있어야 콜백을 받을 수 있음:

```csharp
private Callback<SteamServersConnected_t> steamConnectedCallback;
private Callback<SteamServersDisconnected_t> steamDisconnectedCallback;

void OnEnable()
{
    // Steam 서버 연결 콜백
    steamConnectedCallback = Callback<SteamServersConnected_t>.Create(OnSteamConnected);
    steamDisconnectedCallback = Callback<SteamServersDisconnected_t>.Create(OnSteamDisconnected);
}

void OnSteamConnected(SteamServersConnected_t callback)
{
    Debug.Log("[Steam] 서버 연결됨");
}

void OnSteamDisconnected(SteamServersDisconnected_t callback)
{
    Debug.LogWarning($"[Steam] 서버 연결 끊김! 이유: {callback.m_eResult}");
    // 게임 일시정지, 재접속 UI 표시 등
}

// 또는 주기적으로 수동 체크
void CheckSteamConnection()
{
    if (!SteamUser.BLoggedOn())
    {
        Debug.LogWarning("[Steam] 로그아웃됨!");
    }
}
```

### 요약 표
| 항목 | 자동 체크? | API |
|------|-----------|-----|
| Steam 초기화 | O | `SteamAPI.Init()` |
| 로그인 상태 | X (직접 호출) | `SteamUser.BLoggedOn()` |
| 구매 여부 | X (직접 호출) | `SteamApps.BIsSubscribed()` |
| 연결 끊김 감지 | △ (콜백 등록 필요) | `SteamServersDisconnected_t` |

### 유저 정보 가져오기
```csharp
// Steam ID (고유 식별자)
CSteamID steamId = SteamUser.GetSteamID();
ulong steamId64 = steamId.m_SteamID; // 숫자 형태

// 닉네임
string name = SteamFriends.GetPersonaName();

// 프로필 이미지
int imageId = SteamFriends.GetLargeFriendAvatar(steamId);
// imageId를 Texture2D로 변환하는 코드 필요
```

### 프로필 이미지 가져오기 (선택)
```csharp
public Texture2D GetSteamAvatar(CSteamID steamId)
{
    int imageId = SteamFriends.GetLargeFriendAvatar(steamId);
    if (imageId == -1 || imageId == 0) return null;

    uint width, height;
    if (!SteamUtils.GetImageSize(imageId, out width, out height)) return null;

    byte[] imageData = new byte[width * height * 4];
    if (!SteamUtils.GetImageRGBA(imageId, imageData, (int)(width * height * 4))) return null;

    Texture2D texture = new Texture2D((int)width, (int)height, TextureFormat.RGBA32, false);
    texture.LoadRawTextureData(imageData);
    texture.Apply();

    // Steam 이미지는 상하 반전되어 있으므로 뒤집기 필요
    return FlipTextureVertically(texture);
}

private Texture2D FlipTextureVertically(Texture2D original)
{
    int width = original.width;
    int height = original.height;
    Texture2D flipped = new Texture2D(width, height);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            flipped.SetPixel(x, height - 1 - y, original.GetPixel(x, y));
        }
    }

    flipped.Apply();
    return flipped;
}
```

---

## 5. 서버 인증 (Auth Ticket)

클라이언트에서 발급받은 티켓을 서버로 보내서 검증해야 합니다.

### 클라이언트: 티켓 발급
```csharp
public class SteamAuth : MonoBehaviour
{
    private HAuthTicket currentTicket;
    private byte[] ticketData = new byte[1024];

    // 서버 접속 시 호출
    public string GetAuthTicket()
    {
        if (!SteamManager.Instance.IsInitialized) return null;

        uint ticketSize;
        currentTicket = SteamUser.GetAuthSessionTicket(ticketData, ticketData.Length, out ticketSize);

        if (currentTicket == HAuthTicket.Invalid)
        {
            Debug.LogError("[Steam] 티켓 발급 실패");
            return null;
        }

        // 바이트 배열을 Hex 문자열로 변환
        byte[] actualTicket = new byte[ticketSize];
        System.Array.Copy(ticketData, actualTicket, ticketSize);

        return ByteArrayToHex(actualTicket);
    }

    // 서버 접속 종료 시 호출
    public void CancelAuthTicket()
    {
        if (currentTicket != HAuthTicket.Invalid)
        {
            SteamUser.CancelAuthTicket(currentTicket);
            currentTicket = HAuthTicket.Invalid;
        }
    }

    private string ByteArrayToHex(byte[] bytes)
    {
        System.Text.StringBuilder sb = new System.Text.StringBuilder(bytes.Length * 2);
        foreach (byte b in bytes)
        {
            sb.AppendFormat("{0:X2}", b);
        }
        return sb.ToString();
    }
}
```

### 서버로 전송 예시
```csharp
// 서버 접속 시
void ConnectToGameServer()
{
    string ticket = steamAuth.GetAuthTicket();
    ulong steamId = SteamManager.Instance.SteamID.m_SteamID;

    // 서버로 전송 (기존 패킷 시스템 활용)
    C_SteamAuth authPacket = new C_SteamAuth
    {
        SteamId = steamId,
        AuthTicket = ticket
    };

    Send(authPacket);
}
```

---

## 6. 서버 측 검증

### Steam Web API로 티켓 검증
서버(project-tiamat-server)에서 구현:

```csharp
using System;
using System.Net.Http;
using System.Text.Json;
using System.Threading.Tasks;

public class SteamAuthService
{
    private readonly string _apiKey = "YOUR_STEAM_WEB_API_KEY";
    private readonly uint _appId = 3088400;
    private readonly HttpClient _httpClient = new HttpClient();

    public async Task<SteamAuthResult> VerifyAuthTicket(string ticket, ulong expectedSteamId)
    {
        string url = $"https://api.steampowered.com/ISteamUserAuth/AuthenticateUserTicket/v1/" +
                     $"?key={_apiKey}&appid={_appId}&ticket={ticket}";

        try
        {
            string response = await _httpClient.GetStringAsync(url);
            var json = JsonDocument.Parse(response);

            var responseNode = json.RootElement.GetProperty("response");

            // 에러 체크
            if (responseNode.TryGetProperty("error", out var error))
            {
                return new SteamAuthResult
                {
                    Success = false,
                    ErrorMessage = error.GetProperty("errordesc").GetString()
                };
            }

            var params_ = responseNode.GetProperty("params");
            string result = params_.GetProperty("result").GetString();
            ulong steamId = ulong.Parse(params_.GetProperty("steamid").GetString());

            // Steam ID 일치 확인
            if (steamId != expectedSteamId)
            {
                return new SteamAuthResult
                {
                    Success = false,
                    ErrorMessage = "Steam ID mismatch"
                };
            }

            return new SteamAuthResult
            {
                Success = result == "OK",
                SteamId = steamId,
                OwnsApp = params_.GetProperty("ownersteamid").GetString() == steamId.ToString()
            };
        }
        catch (Exception e)
        {
            return new SteamAuthResult
            {
                Success = false,
                ErrorMessage = e.Message
            };
        }
    }
}

public class SteamAuthResult
{
    public bool Success { get; set; }
    public ulong SteamId { get; set; }
    public bool OwnsApp { get; set; }
    public string ErrorMessage { get; set; }
}
```

### 패킷 핸들러에서 사용
```csharp
// PacketHandler.cs에 추가
public static async void C_SteamAuthHandler(PacketSession session, IMessage packet)
{
    C_SteamAuth authPacket = packet as C_SteamAuth;
    ClientSession clientSession = session as ClientSession;

    var authService = new SteamAuthService();
    var result = await authService.VerifyAuthTicket(authPacket.AuthTicket, authPacket.SteamId);

    if (result.Success)
    {
        clientSession.SteamId = result.SteamId;
        clientSession.IsAuthenticated = true;

        // 인증 성공 응답
        S_SteamAuthResult response = new S_SteamAuthResult { Success = true };
        clientSession.Send(response);
    }
    else
    {
        // 인증 실패 - 연결 끊기
        clientSession.Disconnect();
    }
}
```

---

## 7. 테스트 방법

### 테스트 계정 유형
| 방법 | 대상 | 설명 |
|------|------|------|
| 본인 계정 | 개발자 본인 | Steamworks 파트너 계정 소유자는 자동으로 앱 접근 가능. 별도 설정 불필요 |
| Testers 추가 | 팀원, 지인 | Steamworks에서 Steam 계정 추가 |
| 키 발급 | 외부 테스터, QA | Product Key 발급 후 전달 |

### 방법 1: 본인 계정 (가장 간단)
Steamworks 파트너 계정 소유자는 바로 테스트 가능:
1. `steam_appid.txt`에 `3088400` 입력
2. Steam 클라이언트 로그인
3. Unity 에디터에서 플레이

### 방법 2: Testers 추가
팀원이나 지인 계정 추가:
1. https://partner.steamgames.com 로그인
2. 앱 선택 → **Users & Permissions** → **Manage Testers**
3. **Add Testers** → Steam 계정명 또는 이메일 입력

### 방법 3: 테스트 키 발급
QA나 외부 테스터용:
1. https://partner.steamgames.com 로그인
2. 앱 선택 → **Packages & DLC**
3. 패키지 클릭 → **Request Steam Product Keys**
4. 개수 입력 (보통 10~100개)
5. 발급된 키를 테스터에게 전달

테스터는 Steam에서 "게임 추가 → Steam에서 제품 활성화"로 키 등록

### 테스트 체크리스트
```
[ ] steam_appid.txt에 3088400 입력
[ ] Steam 클라이언트 실행 및 로그인
[ ] Unity 에디터에서 Play
[ ] 콘솔에 "[Steam] 초기화 성공!" 출력 확인
[ ] SteamUser.BLoggedOn() == true 확인
[ ] SteamApps.BIsSubscribed() == true 확인 (구매 여부)
[ ] SteamFriends.GetPersonaName()으로 닉네임 출력 확인
```

### 흔한 에러 및 해결
| 에러 | 원인 | 해결 |
|------|------|------|
| "Steam must be running" | Steam 클라이언트 미실행 | Steam 클라이언트 실행 |
| 초기화 성공, 소유권 없음 | 테스터 미등록 | Testers에 계정 추가 또는 키 등록 |
| DLL 로드 실패 | steam_api64.dll 없음 | Steamworks.NET 재설치 |
| App ID 불일치 | steam_appid.txt 오류 | 3088400으로 수정 |

---

## 8. 빌드 및 배포

### 빌드 시 필요한 파일
빌드 폴더에 다음 파일들이 있어야 함:
```
GameName.exe
GameName_Data/
steam_api64.dll        ← Steamworks.NET에서 자동 복사됨
steam_appid.txt        ← 삭제! (배포 시에는 필요 없음)
```

### 주의사항
1. `steam_appid.txt`는 **배포 빌드에 포함하지 않음**
   - Steam 클라이언트가 App ID를 자동으로 제공함

2. `steam_api64.dll` 위치
   - 32bit 빌드: `steam_api.dll`
   - 64bit 빌드: `steam_api64.dll`

### Steamworks 설정 (partner.steamgames.com)
1. **앱 설정**
   - App Admin → Edit Steamworks Settings
   - Installation → General Installation → 실행 파일 경로 설정

2. **빌드 업로드**
   - SteamPipe를 통해 빌드 업로드
   - `steamcmd` 또는 Steam GUI 도구 사용

---

## 9. 완성 코드

### SteamManager.cs (전체)
모든 기능이 통합된 완성 버전:

```csharp
using Steamworks;
using UnityEngine;

public class SteamManager : MonoBehaviour
{
    public static SteamManager Instance { get; private set; }

    public bool IsInitialized { get; private set; }
    public CSteamID SteamID { get; private set; }
    public string PlayerName { get; private set; }

    // 콜백
    private Callback<SteamServersConnected_t> steamConnectedCallback;
    private Callback<SteamServersDisconnected_t> steamDisconnectedCallback;

    void Awake()
    {
        // 싱글톤
        if (Instance != null)
        {
            Destroy(gameObject);
            return;
        }
        Instance = this;
        DontDestroyOnLoad(gameObject);

        InitializeSteam();
    }

    void InitializeSteam()
    {
        // Steam 클라이언트 실행 체크
        if (SteamAPI.RestartAppIfNecessary(new AppId_t(3088400)))
        {
            Debug.Log("Steam 클라이언트를 통해 재시작 필요");
            Application.Quit();
            return;
        }

        try
        {
            IsInitialized = SteamAPI.Init();

            if (!IsInitialized)
            {
                Debug.LogError("[Steam] 초기화 실패! Steam 클라이언트가 실행 중인지 확인하세요.");
                return;
            }

            // 유저 정보 캐싱
            SteamID = SteamUser.GetSteamID();
            PlayerName = SteamFriends.GetPersonaName();

            // 콜백 등록
            steamConnectedCallback = Callback<SteamServersConnected_t>.Create(OnSteamConnected);
            steamDisconnectedCallback = Callback<SteamServersDisconnected_t>.Create(OnSteamDisconnected);

            Debug.Log($"[Steam] 초기화 성공! 유저: {PlayerName} ({SteamID})");

            // 구매 여부 체크
            if (!OwnsGame())
            {
                Debug.LogError("[Steam] 게임을 소유하고 있지 않음!");
                Application.Quit();
                return;
            }
        }
        catch (System.DllNotFoundException e)
        {
            Debug.LogError($"[Steam] DLL 로드 실패: {e.Message}");
            Debug.LogError("steam_api64.dll이 빌드 폴더에 있는지 확인하세요.");
        }
    }

    void Update()
    {
        if (IsInitialized)
        {
            SteamAPI.RunCallbacks(); // Steam 이벤트 처리 (필수!)
        }
    }

    void OnApplicationQuit()
    {
        if (IsInitialized)
        {
            SteamAPI.Shutdown();
        }
    }

    #region 로그인 및 소유권

    // Steam에 로그인되어 있는지 확인
    public bool IsLoggedIn()
    {
        return IsInitialized && SteamUser.BLoggedOn();
    }

    // 현재 앱을 소유하고 있는지 (구매했는지)
    public bool OwnsGame()
    {
        if (!IsInitialized) return false;
        return SteamApps.BIsSubscribed();
    }

    // 특정 App ID로 체크 (DLC 등 확인할 때 유용)
    public bool OwnsApp(uint appId)
    {
        if (!IsInitialized) return false;
        return SteamApps.BIsSubscribedToApp(new AppId_t(appId));
    }

    #endregion

    #region 콜백

    void OnSteamConnected(SteamServersConnected_t callback)
    {
        Debug.Log("[Steam] 서버 연결됨");
    }

    void OnSteamDisconnected(SteamServersDisconnected_t callback)
    {
        Debug.LogWarning($"[Steam] 서버 연결 끊김! 이유: {callback.m_eResult}");
        // TODO: 게임 일시정지, 재접속 UI 표시 등
    }

    #endregion

    #region 아바타 (선택)

    public Texture2D GetSteamAvatar(CSteamID steamId)
    {
        int imageId = SteamFriends.GetLargeFriendAvatar(steamId);
        if (imageId == -1 || imageId == 0) return null;

        uint width, height;
        if (!SteamUtils.GetImageSize(imageId, out width, out height)) return null;

        byte[] imageData = new byte[width * height * 4];
        if (!SteamUtils.GetImageRGBA(imageId, imageData, (int)(width * height * 4))) return null;

        Texture2D texture = new Texture2D((int)width, (int)height, TextureFormat.RGBA32, false);
        texture.LoadRawTextureData(imageData);
        texture.Apply();

        return FlipTextureVertically(texture);
    }

    private Texture2D FlipTextureVertically(Texture2D original)
    {
        int width = original.width;
        int height = original.height;
        Texture2D flipped = new Texture2D(width, height);

        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                flipped.SetPixel(x, height - 1 - y, original.GetPixel(x, y));
            }
        }

        flipped.Apply();
        return flipped;
    }

    #endregion
}
```

### SteamAuth.cs (전체)
서버 인증용 티켓 관리:

```csharp
using Steamworks;
using UnityEngine;

public class SteamAuth : MonoBehaviour
{
    private HAuthTicket currentTicket;
    private byte[] ticketData = new byte[1024];

    // 서버 접속 시 호출
    public string GetAuthTicket()
    {
        if (!SteamManager.Instance.IsInitialized) return null;

        uint ticketSize;
        currentTicket = SteamUser.GetAuthSessionTicket(ticketData, ticketData.Length, out ticketSize);

        if (currentTicket == HAuthTicket.Invalid)
        {
            Debug.LogError("[Steam] 티켓 발급 실패");
            return null;
        }

        byte[] actualTicket = new byte[ticketSize];
        System.Array.Copy(ticketData, actualTicket, ticketSize);

        return ByteArrayToHex(actualTicket);
    }

    // 서버 접속 종료 시 호출
    public void CancelAuthTicket()
    {
        if (currentTicket != HAuthTicket.Invalid)
        {
            SteamUser.CancelAuthTicket(currentTicket);
            currentTicket = HAuthTicket.Invalid;
        }
    }

    private string ByteArrayToHex(byte[] bytes)
    {
        System.Text.StringBuilder sb = new System.Text.StringBuilder(bytes.Length * 2);
        foreach (byte b in bytes)
        {
            sb.AppendFormat("{0:X2}", b);
        }
        return sb.ToString();
    }
}
```

---

## 체크리스트

### 개발 단계
- [ ] Steamworks.NET 설치
- [ ] steam_appid.txt 생성 (3088400)
- [ ] SteamManager.cs 구현
- [ ] Steam 클라이언트 실행 후 테스트
- [ ] 로그인 상태 확인 (`IsLoggedIn()`)
- [ ] 구매 여부 확인 (`OwnsGame()`)
- [ ] 연결 끊김 콜백 테스트

### 서버 연동
- [ ] SteamAuth.cs 구현 (Auth Ticket 발급)
- [ ] 서버 SteamAuthService 구현 (Web API 검증)
- [ ] Proto 파일에 인증 패킷 추가 (C_SteamAuth, S_SteamAuthResult)

### 배포 단계
- [x] Steamworks 파트너 사이트 앱 등록 (완료)
- [x] 실제 App ID 확인 (3088400)
- [ ] Web API Key 발급 및 서버 설정
- [ ] steam_appid.txt 빌드에서 제외
- [ ] SteamPipe로 빌드 업로드

---

## 참고 자료
- [Steamworks.NET 공식 문서](https://steamworks.github.io/)
- [Steamworks API 레퍼런스](https://partner.steamgames.com/doc/api)
- [Steam Web API](https://partner.steamgames.com/doc/webapi)
