/* Shared human date presentation for bundled web pages and documentation. */
(function (global) {
  'use strict';
  var months = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
  // Calendar dates are civil values; timestamps represent viewer-local instants.
  function format(value, mode, includeTime) {
    var text = String(value || '');
    var calendar = /^(\d{4})-(\d{2})-(\d{2})$/.exec(text);
    if (!calendar && !/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d+)?(?:Z|[+-]\d{2}:\d{2})$/.test(text)) return '';
    var civil = /^(\d{4})-(\d{2})-(\d{2})/.exec(text);
    var check = new Date(0);
    check.setUTCFullYear(Number(civil[1]), Number(civil[2]) - 1, Number(civil[3]));
    if (check.getUTCFullYear() !== Number(civil[1]) || check.getUTCMonth() + 1 !== Number(civil[2]) || check.getUTCDate() !== Number(civil[3])) return '';
    var date = calendar ? check : new Date(text);
    if (!Number.isFinite(date.getTime())) return '';
    var year = calendar ? date.getUTCFullYear() : date.getFullYear();
    var month = calendar ? date.getUTCMonth() : date.getMonth();
    var day = calendar ? date.getUTCDate() : date.getDate();
    if (calendar && (year !== Number(calendar[1]) || month + 1 !== Number(calendar[2]) || day !== Number(calendar[3]))) return '';
    var en = day + ' ' + months[month] + ' ' + year;
    var yue = year + '年' + (month + 1) + '月' + day + '日';
    mode = mode || (typeof global.GetCurrentWebLang === 'function' ? global.GetCurrentWebLang() : 'en');
    var result = mode === 'yue_HK' ? yue : mode === 'bilingual_en_yue_HK' ? en + ' / ' + yue : en;
    if (includeTime && !calendar) result += ' ' + String(date.getHours()).padStart(2, '0') + ':' + String(date.getMinutes()).padStart(2, '0');
    return result;
  }

  // Local civil timestamp fields are a separate, explicit source contract.
  function formatLocal(value, mode, includeTime) {
    var text = String(value || '');
    if (/^\d{4}-\d{2}-\d{2}$/.test(text) || /(?:Z|[+-]\d{2}:\d{2})$/.test(text)) return format(text, mode, includeTime);
    var parts = /^(\d{4})-(\d{2})-(\d{2})[ T](\d{2}):(\d{2})(?::(\d{2}))?$/.exec(text);
    if (!parts) return '';
    var date = new Date(Number(parts[1]), Number(parts[2]) - 1, Number(parts[3]), Number(parts[4]), Number(parts[5]), Number(parts[6] || 0));
    if (date.getFullYear() !== Number(parts[1]) || date.getMonth() + 1 !== Number(parts[2]) || date.getDate() !== Number(parts[3]) || date.getHours() !== Number(parts[4]) || date.getMinutes() !== Number(parts[5])) return '';
    return format(date.toISOString(), mode, includeTime);
  }

  function present(en, yue, mode) {
    mode = mode || (typeof global.GetCurrentWebLang === 'function' ? global.GetCurrentWebLang() : 'en');
    return mode === 'yue_HK' ? yue : mode === 'bilingual_en_yue_HK' ? en + ' / ' + yue : en;
  }
  function monthName(month, mode) {
    return month >= 0 && month < 12 ? present(months[month], (month + 1) + '月', mode) : '';
  }
  function monthYear(year, month, mode) {
    return month >= 0 && month < 12 ? present(months[month] + ' ' + year, year + '年' + (month + 1) + '月', mode) : '';
  }
  global.BambuHumanDate = { format: format, formatLocal: formatLocal, monthName: monthName, monthYear: monthYear };
})(typeof window !== 'undefined' ? window : globalThis);
