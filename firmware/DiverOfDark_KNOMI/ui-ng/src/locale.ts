import { writable } from "svelte/store";
import type { Locale } from "./i18n";

// Read initial locale from localStorage, default to browser language or English
function getInitialLocale(): Locale {
  try {
    const stored = localStorage.getItem("knomi_locale");
    if (stored === "en" || stored === "zh") return stored;
  } catch {
    // localStorage might be unavailable
  }
  // Default to Chinese if browser language is zh, otherwise English
  if (navigator.language?.startsWith("zh")) return "zh";
  return "en";
}

export const locale = writable<Locale>(getInitialLocale());

// Persist to localStorage on change
locale.subscribe((val) => {
  try {
    localStorage.setItem("knomi_locale", val);
  } catch {
    // ignore
  }
});
