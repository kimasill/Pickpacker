# Linear + GitHub Minimum Integration

이 저장소에는 Linear 이슈를 바로 만들고, GitHub PR에서 Linear 이슈 ID를 강제하는 최소 구성이 들어가 있습니다.

## 포함된 구성

- `Scripts/Create-LinearIssue.ps1`
  - Linear GraphQL API로 이슈를 생성합니다.
  - 원하면 생성 직후 로컬 git 브랜치도 만듭니다.
- `.github/workflows/create-linear-issue.yml`
  - GitHub Actions의 `workflow_dispatch`로 Linear 이슈를 생성합니다.
- `.github/workflows/require-linear-id.yml`
  - PR의 브랜치명, 제목, 본문에 Linear 이슈 ID가 없으면 실패시킵니다.
- `.github/PULL_REQUEST_TEMPLATE.md`
  - PR에 Linear 이슈 ID를 적는 기본 템플릿입니다.

## 사전 설정

### 1. Linear 쪽 설정

Linear 워크스페이스 관리자 권한으로 다음을 켭니다.

1. `Settings -> Integrations -> GitHub`
2. 이 저장소가 있는 GitHub organization/repository 연결
3. `Pull request linking` 활성화
4. 필요하면 `Commit linking`, `Issue status automation`도 활성화

공식 문서:

- https://linear.app/docs/github-integration
- https://linear.app/integrations/github

Linear API 키는 `Settings -> Account -> Security & Access`에서 생성합니다.
이 스크립트는 Personal API Key 기준으로 동작합니다.

### 2. GitHub 쪽 설정

이 저장소의 `Settings -> Secrets and variables -> Actions`에 다음을 추가합니다.

#### Secrets

- `LINEAR_API_KEY`
  - Linear Personal API Key

#### Variables

- `LINEAR_TEAM_ID`
  - 이슈를 생성할 Linear 팀 UUID
  - Linear에서 `Cmd/Ctrl + K -> Copy model UUID`로 확인 가능
- `LINEAR_WORKSPACE_URL`
  - 예: `pickpacker`
  - 또는 전체 URL `https://linear.app/pickpacker`

공식 문서:

- https://linear.app/docs/api-and-webhooks
- https://linear.app/developers/graphql
- https://docs.github.com/actions/security-guides/using-secrets-in-github-actions
- https://docs.github.com/actions/using-workflows/triggering-a-workflow

## 사용 방법

### 0. 로컬 터미널용 환경변수 설정

GitHub secret/variable을 넣어도 로컬 터미널은 그 값을 자동으로 읽지 않습니다.
이 저장소의 에이전트가 로컬에서 바로 Linear 이슈를 만들게 하려면 Windows 사용자 환경변수에도 같은 값을 넣어야 합니다.

현재 열려 있는 PowerShell 세션에서 실행:

```powershell
.\Scripts\Set-LinearEnv.ps1
```

실행하면 아래 3개를 입력받아 저장합니다.

- `LINEAR_API_KEY`
- `LINEAR_TEAM_ID`
- `LINEAR_WORKSPACE_URL`

기본값은 `User` 스코프라서 현재 Windows 사용자 계정에 저장됩니다.
지금 세션에서 실행하면 현재 터미널에도 바로 반영되고, 새 터미널을 열어도 계속 사용할 수 있습니다.

확인은 아래 명령으로 합니다.

```powershell
.\Scripts\Test-LinearEnv.ps1
```

### A. 로컬에서 Linear 이슈 만들고 바로 브랜치 시작

PowerShell:

```powershell
pwsh -File .\Scripts\Create-LinearIssue.ps1 `
  -Title "Fix missing character assets" `
  -Description "Character blueprints and dependent assets need recovery." `
  -CreateBranch
```

환경 변수:

- `LINEAR_API_KEY`
- `LINEAR_TEAM_ID`
- 선택: `LINEAR_WORKSPACE_URL`

명시적으로 넣고 싶으면:

```powershell
pwsh -File .\Scripts\Create-LinearIssue.ps1 `
  -Title "Fix missing character assets" `
  -Description "Character blueprints and dependent assets need recovery." `
  -TeamId "00000000-0000-0000-0000-000000000000" `
  -ApiKey "lin_api_xxx" `
  -CreateBranch
```

성공하면 다음 정보를 출력합니다.

- Linear 이슈 ID
- 제목
- 추천 브랜치명
- 선택 시 Linear 이슈 URL

브랜치명은 `PP-123-fix-missing-character-assets` 형식으로 만들어집니다.

### B. GitHub Actions에서 Linear 이슈 만들기

1. GitHub 저장소 `Actions`
2. `Create Linear Issue`
3. `title`, `description` 입력
4. 필요하면 `team_id` override 입력

워크플로 실행 후 summary에 다음이 출력됩니다.

- 생성된 Linear 이슈 ID
- 추천 브랜치명
- 이슈 URL

## 운영 규칙

### 브랜치 규칙

Linear 이슈 ID를 브랜치명에 넣습니다.

예:

- `PP-123-fix-order-tracker`
- `PP-456-recover-character-blueprints`

### PR 규칙

다음 셋 중 하나에는 반드시 Linear 이슈 ID가 있어야 합니다.

- 브랜치명
- PR 제목
- PR 본문

이 저장소에서는 `.github/workflows/require-linear-id.yml`이 이를 검사합니다.

## 권장 운영 흐름

1. Linear 이슈 생성
2. 생성된 이슈 ID로 브랜치 시작
3. 작업 후 PR 생성
4. Linear GitHub integration이 PR을 이슈에 자동 연결
5. 머지 시 Linear 상태 자동화 사용 가능

## 제한 사항

- 이 저장소에 추가한 것은 "저장소 쪽 최소 구현"입니다.
- 실제 PR/커밋 링크 표시는 Linear 워크스페이스에서 GitHub integration을 켜야 동작합니다.
- 자동 브랜치 생성은 로컬 스크립트에서만 지원합니다. GitHub Actions의 수동 이슈 생성 워크플로는 브랜치명만 제안합니다.
