# AOS session manager

The installed-system session boundary is owned by the kernel and presented by
MUI. Live media remains immediately usable, while an installed system starts
MUI under the restricted `login` identity.

## Boot flow

1. The kernel checks for `etc/aos-install.conf`.
2. Live mode keeps its existing root development session.
3. An installed system starts PID 1 as UID/GID 65534.
4. MUI queries syscall `563` and renders the native sign-in screen.
5. A successful login or enabled autologin changes PID 1 to UID/GID 1000,
   username from `etc/aos-system.conf`, and home `/main`.
6. Logout terminates child processes owned by UID 1000, clears MUI session
   state, and restores the restricted login identity.

Only PID 1 can request login, autologin, or logout. Other processes can query
session status but cannot replace the active identity.

## Credentials

New installations require passwords of at least eight characters. The installer
stores a random 16-byte salt and a 32-byte PBKDF2-HMAC-SHA256 result in
`etc/shadow`:

```text
$aos-pbkdf2-sha256$12000$<salt>$<digest>
```

Password comparison is constant-time and temporary password/hash buffers are
wiped after use. The verifier accepts the older `$aos$` format so existing
development images can still boot, but the installer only writes PBKDF2 records.

After three failed login attempts, the kernel applies an increasing retry delay,
capped at ten seconds.

## Accounts and permissions

- Administrator: may authenticate with `sudo`; the command receives effective
  UID/GID 0.
- Standard: `sudo` is rejected before asking for a password.
- Installed users: may write under `/main`, `/tmp`, and their trash area.
- System paths such as `/etc` remain read-only to an unelevated user.

The MUI Settings Users page and Terminal prompt read the active session status
instead of assuming an account name.

## User controls

Use **Sign Out** in the MUI top-bar power menu or press `Ctrl+Alt+L`. Shutdown
and restart remain available at the sign-in screen.

## ABI

The versioned public ABI is in `include/aos_session.h`:

- `AOS_SESSION_QUERY`
- `AOS_SESSION_LOGIN`
- `AOS_SESSION_AUTOLOGIN`
- `AOS_SESSION_LOGOUT`

Status reports live/installed mode, authentication state, autologin, account
type, UID/GID, failed attempts, retry delay, username, home, and a display
message.
