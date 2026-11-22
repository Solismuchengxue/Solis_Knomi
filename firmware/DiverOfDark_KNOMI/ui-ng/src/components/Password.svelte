<script lang="ts">
    import { onMount } from "svelte";
    export let lang: "en" | "zh" = "en";
    let configured = false;
    let password = "";
    let confirmation = "";
    let busy = false;
    let failed = false;
    onMount(async () => {
        try {
            const response = await fetch("/api/auth/status", { cache: "no-store" });
            if (!response.ok) { failed = true; return; }
            configured = (await response.json()).configured;
        } catch { failed = true; }
    });
    async function save() {
        failed = false;
        if (password !== confirmation) { failed = true; return; }
        busy = true;
        try {
            const response = await fetch("/api/auth/password", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ password }),
            });
            password = "";
            confirmation = "";
            if (response.ok) window.location.reload();
            else failed = true;
        } catch { password = ""; confirmation = ""; failed = true; }
        finally { busy = false; }
    }
</script>

<form on:submit|preventDefault={save}>
    <h3>{lang === "zh" ? (configured ? "修改管理密码" : "设置管理密码") : (configured ? "Change administrator password" : "Set administrator password")}</h3>
    <p>{lang === "zh" ? "登录用户名固定为 admin。未设密码时 WebUI 开放；保存后所有入口需要认证，页面重新加载时请使用新密码。" : "Login username: admin. WebUI is open until a password is set. After saving, all entry points require authentication; use the new password when the page reloads."}</p>
    <label>
        {lang === "zh" ? "新密码（8–128 UTF-8 字节）" : "New password (8–128 UTF-8 bytes)"}
        <input type="password" autocomplete="new-password" minlength="8" maxlength="128" required bind:value={password} disabled={busy} />
    </label>
    <label>
        {lang === "zh" ? "再次输入新密码" : "Confirm new password"}
        <input type="password" autocomplete="new-password" minlength="8" maxlength="128" required bind:value={confirmation} disabled={busy} />
    </label>
    {#if failed}<p role="alert">{lang === "zh" ? "未保存：请检查两次密码、长度和连接后重试。" : "Not saved: check matching passwords, length and connection, then retry."}</p>{/if}
    <button type="submit" disabled={busy}>{lang === "zh" ? "保存密码" : "Save password"}</button>
</form>
