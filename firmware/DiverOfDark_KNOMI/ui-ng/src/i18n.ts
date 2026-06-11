// i18n dictionary — English & Chinese for KNOMI Web UI

export type Locale = "en" | "zh";

type Dict = Record<string, string | ((...args: any[]) => string)>;

const en: Record<string, string> = {
  /* Navigation */
  nav_setup: "Setup",
  nav_update: "Update",
  nav_theme: "Theme",

  /* Setup form */
  wifi_ssid: "WiFi SSID",
  wifi_pass: "WiFi PASS",
  klipper_ip: "Klipper IP",
  knomi_hostname: "KNOMI Hostname",
  progress_method: "Print Progress calculation method",
  progress_file_relative: "File Relative (default)",
  progress_file_absolute: "File Absolute",
  progress_slicer: "Slicer",
  progress_filament: "Filament",
  skip_standby: "Skip alternating standby and voron images during standby",
  save: "SAVE",

  /* Setup done */
  submit_success: "Submission successful!",
  close_page: "You may now close this page.",

  /* OTA */
  ota_success: "OTA Success",
  ota_back: "Back",
  ota_retry: "Retry",
  ota_progress: "Update is in progress. Please do not close this page.",
  ota_select: "Select what you want to update:",
  ota_firmware: "Firmware",
  ota_theme: "Theme",
  ota_choose: "Choose Firmware",
  ota_upload: "Upload",

  /* Footer */
  footer_lovely: "Sincerely, your lovely {device} display.",
  footer_built_from: "Firmware built from <b>{branch}</b> at <b>{buildTimestamp}</b>.",
  footer_commit_tip: "Commit {hash} from {gitTimestamp}",
  footer_check_repo: "Check <a href=\"{url}\" target=\"_blank\">repository</a> for more details.",
  footer_update_to: "Update to <a href=\"{url}\" target=\"_blank\">{version}</a> available.",

  /* Theme page */
  loading: "Loading...",
  ui_bg_color: "UI Background Color",
  ui_accent_color: "UI Accent Color",
  filename: "Filename",
  size: "Size",
  missing: "MISSING",
  used_space: "Used space",

  /* Theme file */
  back: "Back",
  current: "Current",
  updating: "updating",
  not_found: "NOT FOUND",
  original: "Original",
  upload_new: "Upload new image",

  /* Error messages */
  err_no_space: "Not enough free space for this file. Available is ",
  err_file_is: ", file is ",
  err_bad_format: "Not supported file format! Expected '",
  err_but_got: "', but got '",
  err_quote_end: "'",
  err_unknown: "Unknown error while upload, check the console for details.",
};

const zh: Record<string, string> = {
  /* Navigation */
  nav_setup: "设置",
  nav_update: "更新",
  nav_theme: "主题",

  /* Setup form */
  wifi_ssid: "WiFi SSID",
  wifi_pass: "WiFi 密码",
  klipper_ip: "Klipper IP 地址",
  knomi_hostname: "KNOMI 主机名",
  progress_method: "打印进度计算方式",
  progress_file_relative: "文件相对（默认）",
  progress_file_absolute: "文件绝对",
  progress_slicer: "切片器",
  progress_filament: "耗材",
  skip_standby: "待机时跳过交替显示待机和Voron图像",
  save: "保存",

  /* Setup done */
  submit_success: "提交成功！",
  close_page: "您现在可以关闭此页面。",

  /* OTA */
  ota_success: "OTA 成功",
  ota_back: "返回",
  ota_retry: "重试",
  ota_progress: "更新正在进行中，请勿关闭此页面。",
  ota_select: "选择您要更新的内容：",
  ota_firmware: "固件",
  ota_theme: "主题",
  ota_choose: "选择固件",
  ota_upload: "上传",

  /* Footer */
  footer_lovely: "您可爱的 {device} 显示器敬上。",
  footer_built_from: "固件构建自 <b>{branch}</b> 于 <b>{buildTimestamp}</b>。",
  footer_commit_tip: "提交 {hash} 于 {gitTimestamp}",
  footer_check_repo: "前往 <a href=\"{url}\" target=\"_blank\">仓库</a> 了解更多详情。",
  footer_update_to: "更新到 <a href=\"{url}\" target=\"_blank\">{version}</a> 可用。",

  /* Theme page */
  loading: "加载中...",
  ui_bg_color: "UI 背景颜色",
  ui_accent_color: "UI 强调颜色",
  filename: "文件名",
  size: "大小",
  missing: "缺失",
  used_space: "已用空间",

  /* Theme file */
  back: "返回",
  current: "当前",
  updating: "更新中",
  not_found: "未找到",
  original: "原始",
  upload_new: "上传新图片",

  /* Error messages */
  err_no_space: "此文件空间不足。可用空间为 ",
  err_file_is: "，文件大小为 ",
  err_bad_format: "不支持的文件格式！期望 '",
  err_but_got: "'，但收到 '",
  err_quote_end: "'",
  err_unknown: "未知上传错误，请查看控制台获取详细信息。",
};

const locales: Record<Locale, Record<string, string>> = { en, zh };

export function t(key: string, locale: Locale, vars?: Record<string, string | number>): string {
  const dict = locales[locale];
  let val = dict[key] ?? key;
  if (vars) {
    for (const [k, v] of Object.entries(vars)) {
      val = val.replace(`{${k}}`, String(v));
    }
  }
  return val;
}
