# MeyerScan Database Change Log

## 2026-06-22

- Added module-level change log.
- Reconfirmed Database boundary: connection, transaction, SQL execution, backup, MySQL/SQLite adaptation only.
- Business schema, migrations, case/order semantics, permissions, and UI must stay outside this module.
- Existing hardcoded MySQL credential and backup-path TODOs remain deferred until ConfigCenter is available; avoid adding a partial configuration path before the final config owner exists.
- Explicitly removes Qt SQL connections from the Qt connection pool on `Disconnect()` / database-type switch to avoid plugin/lifecycle issues when UI smoke hosts exit.

## 2026-06-18

- Confirmed old `MyCaseManager/mysql.sql` is not loaded by Database.
- Documented `mysql.sql` as a legacy schema reference only.
- Kept `ExecuteScript()` as a generic execution primitive; schema versioning and migration selection belong to ConfigCenter or service modules.

## 2026-06-17

- Aligned interfaces to `Result<T>` / `VoidResult`.
- Integrated Logger through runtime dynamic loading.
- Fixed `SetDatabaseType()` recursive lock risk.
- Fixed SQL error message lifetime issue.
- Verified Release x64 build and `DatabaseTest`.
