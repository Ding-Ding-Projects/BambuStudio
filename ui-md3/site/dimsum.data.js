/*
 * The dim sum surprise catalogue.
 *
 * Artwork is hand-authored inline SVG so a dish costs no network request, no
 * third-party CDN and no tracking pixel — the whole point of a small delight is
 * that it stays small. Every dish carries its English and Cantonese name and an
 * alt text naming the dish, so a screen-reader user gets the same surprise.
 *
 * Food colours are data, not chrome, so they are deliberately fixed rather than
 * derived from the active Material palette; a har gow tinted by the accent seed
 * stops being a har gow.
 */
(function (global) {
  'use strict';

  var basket =
    '<ellipse cx="60" cy="82" rx="46" ry="18" fill="#c8a06a"/>' +
    '<ellipse cx="60" cy="78" rx="46" ry="18" fill="#e0bd8c"/>' +
    '<ellipse cx="60" cy="76" rx="38" ry="13" fill="#f0d7b0"/>';

  var plate =
    '<ellipse cx="60" cy="84" rx="48" ry="16" fill="#b9c3d6"/>' +
    '<ellipse cx="60" cy="80" rx="48" ry="16" fill="#dde4ef"/>' +
    '<ellipse cx="60" cy="78" rx="36" ry="10" fill="#f4f7fc"/>';

  var dishes = [
    {
      id: 'har-gow',
      en: 'Shrimp dumpling',
      yue: '蝦餃',
      art:
        basket +
        '<g>' +
        '<path d="M22 74c0-13 9-23 22-23s22 10 22 23z" fill="#f7f4ee"/>' +
        '<path d="M28 62c4 3 8 3 12 0m2-6c4 3 8 3 12 0" stroke="#ded6c6" stroke-width="2.4" fill="none" stroke-linecap="round"/>' +
        '<path d="M33 68c5-6 15-7 20-1" stroke="#f2a2a2" stroke-width="5" fill="none" stroke-linecap="round"/>' +
        '<path d="M54 74c0-13 9-23 22-23s22 10 22 23z" fill="#fdfbf6"/>' +
        '<path d="M60 62c4 3 8 3 12 0m2-6c4 3 8 3 12 0" stroke="#ded6c6" stroke-width="2.4" fill="none" stroke-linecap="round"/>' +
        '<path d="M65 68c5-6 15-7 20-1" stroke="#f08f8f" stroke-width="5" fill="none" stroke-linecap="round"/>' +
        '</g>'
    },
    {
      id: 'siu-mai',
      en: 'Pork and shrimp dumpling',
      yue: '燒賣',
      art:
        basket +
        '<g>' +
        '<path d="M26 50h34v18a8 8 0 0 1-8 8H34a8 8 0 0 1-8-8z" fill="#f3d774"/>' +
        '<ellipse cx="43" cy="50" rx="17" ry="8" fill="#c9825f"/>' +
        '<circle cx="43" cy="48" r="4" fill="#e8552f"/>' +
        '<path d="M62 54h32v16a8 8 0 0 1-8 8H70a8 8 0 0 1-8-8z" fill="#efd069"/>' +
        '<ellipse cx="78" cy="54" rx="16" ry="7.5" fill="#bd7856"/>' +
        '<circle cx="78" cy="52" r="3.6" fill="#e04d29"/>' +
        '</g>'
    },
    {
      id: 'char-siu-bao',
      en: 'Barbecue pork bun',
      yue: '叉燒包',
      art:
        basket +
        '<g>' +
        '<circle cx="44" cy="58" r="20" fill="#fbf8f4"/>' +
        '<path d="M34 48c4 6 5 12 3 18m9-20c1 7 1 13-1 19m10-19c-2 6-2 12 1 18" stroke="#e6ded4" stroke-width="2.4" fill="none" stroke-linecap="round"/>' +
        '<path d="M36 50c5-5 12-6 17-2-4 3-9 4-17 2z" fill="#a75434"/>' +
        '<circle cx="76" cy="62" r="17" fill="#fdfbf8"/>' +
        '<path d="M68 54c3 5 4 10 2 15m8-17c1 6 1 11-1 16" stroke="#e6ded4" stroke-width="2.2" fill="none" stroke-linecap="round"/>' +
        '</g>'
    },
    {
      id: 'cheung-fun',
      en: 'Rice noodle roll',
      yue: '腸粉',
      art:
        plate +
        '<g>' +
        '<rect x="22" y="52" width="76" height="15" rx="7.5" fill="#fbfaf7"/>' +
        '<rect x="26" y="63" width="72" height="15" rx="7.5" fill="#f6f4ef"/>' +
        '<path d="M30 52v15m14-15v15m14-15v15m14-15v15m14-15v15" stroke="#e7e3da" stroke-width="2" />' +
        '<path d="M24 76c22 6 50 6 72 0" stroke="#6b4423" stroke-width="4" fill="none" stroke-linecap="round" opacity=".85"/>' +
        '<circle cx="46" cy="72" r="2" fill="#3d2a17"/><circle cx="70" cy="74" r="2" fill="#3d2a17"/>' +
        '</g>'
    },
    {
      id: 'egg-tart',
      en: 'Egg tart',
      yue: '蛋撻',
      art:
        plate +
        '<g>' +
        '<path d="M24 56h56l-6 20a6 6 0 0 1-6 4H36a6 6 0 0 1-6-4z" fill="#e8c485"/>' +
        '<ellipse cx="52" cy="56" rx="28" ry="9" fill="#f6cf6a"/>' +
        '<ellipse cx="52" cy="55" rx="22" ry="6.5" fill="#f9dd8d"/>' +
        '<path d="M26 58c4 4 8 6 12 6m8 2c6 2 12 2 18 0m8-4c4-1 7-3 10-6" stroke="#d8ab6a" stroke-width="2" fill="none" stroke-linecap="round"/>' +
        '<ellipse cx="86" cy="70" rx="16" ry="7" fill="#eec98d"/>' +
        '<ellipse cx="86" cy="68" rx="12" ry="4.6" fill="#f8dc90"/>' +
        '</g>'
    },
    {
      id: 'lo-mai-gai',
      en: 'Lotus leaf sticky rice',
      yue: '糯米雞',
      art:
        basket +
        '<g>' +
        '<path d="M24 70c2-18 16-28 36-28s34 10 36 28z" fill="#5d8a4a"/>' +
        '<path d="M30 68c2-14 13-22 30-22s28 8 30 22z" fill="#6d9c56"/>' +
        '<path d="M60 46v22M60 52c-8 2-13 6-16 12m16-6c8 2 13 6 16 12" stroke="#4c7a3c" stroke-width="2.2" fill="none" stroke-linecap="round"/>' +
        '<path d="M46 44c6-6 22-6 28 0-6 3-22 3-28 0z" fill="#7fae66"/>' +
        '</g>'
    },
    {
      id: 'turnip-cake',
      en: 'Turnip cake',
      yue: '蘿蔔糕',
      art:
        plate +
        '<g>' +
        '<rect x="24" y="48" width="42" height="28" rx="4" fill="#e7d9bd"/>' +
        '<rect x="24" y="48" width="42" height="8" rx="4" fill="#d9b878"/>' +
        '<circle cx="36" cy="64" r="3" fill="#c9a978"/><circle cx="50" cy="60" r="2.6" fill="#c9a978"/>' +
        '<circle cx="56" cy="69" r="2.2" fill="#b9946a"/>' +
        '<rect x="60" y="56" width="38" height="24" rx="4" fill="#efe2c8"/>' +
        '<rect x="60" y="56" width="38" height="7" rx="4" fill="#dcbd80"/>' +
        '<circle cx="72" cy="70" r="2.6" fill="#c9a978"/><circle cx="86" cy="67" r="2.2" fill="#c9a978"/>' +
        '</g>'
    },
    {
      id: 'egg-waffle',
      en: 'Egg waffle',
      yue: '雞蛋仔',
      art:
        '<g>' +
        '<path d="M40 96c-6-10-6-24 2-36 8-13 24-20 38-16 12 3 18 14 16 26-3 16-16 28-30 30z" fill="#f2c86f"/>' +
        '<g fill="#e6b355">' +
        '<circle cx="52" cy="62" r="7"/><circle cx="68" cy="56" r="7"/><circle cx="82" cy="62" r="6.4"/>' +
        '<circle cx="58" cy="76" r="7"/><circle cx="74" cy="72" r="6.6"/><circle cx="64" cy="88" r="6"/>' +
        '</g>' +
        '<path d="M30 92c6 6 14 8 22 6" stroke="#d7a34c" stroke-width="3" fill="none" stroke-linecap="round"/>' +
        '</g>'
    },
    {
      id: 'phoenix-claw',
      en: 'Braised chicken feet',
      yue: '鳳爪',
      art:
        basket +
        '<g stroke="#8d3f22" stroke-width="6" fill="none" stroke-linecap="round">' +
        '<path d="M32 72c6-8 10-14 10-22m-10 22c-2-9-2-16 2-22m-2 22c-7-6-11-11-12-18"/>' +
        '<path d="M74 74c6-8 10-14 10-22m-10 22c-2-9-2-16 2-22m-2 22c-7-6-11-11-12-18"/>' +
        '</g>' +
        '<g stroke="#a95b32" stroke-width="2.4" fill="none" stroke-linecap="round">' +
        '<path d="M32 70c5-7 9-13 9-20m42 22c5-7 9-13 9-20"/>' +
        '</g>' +
        '<circle cx="52" cy="70" r="3" fill="#5f3116"/><circle cx="60" cy="66" r="2.6" fill="#5f3116"/>'
    },
    {
      id: 'mango-pomelo',
      en: 'Mango pomelo sago',
      yue: '楊枝甘露',
      art:
        plate +
        '<g>' +
        '<path d="M38 40h44l-6 34a10 10 0 0 1-10 8H54a10 10 0 0 1-10-8z" fill="#f7c85a"/>' +
        '<path d="M40 46h40l-1 8H41z" fill="#fbdb86"/>' +
        '<g fill="#fdf3d8"><circle cx="52" cy="62" r="3"/><circle cx="62" cy="68" r="2.6"/><circle cx="72" cy="60" r="2.8"/>' +
        '<circle cx="58" cy="76" r="2.4"/><circle cx="68" cy="78" r="2.2"/></g>' +
        '<path d="M50 36c6-6 14-6 20 0-6 4-14 4-20 0z" fill="#f0a03c"/>' +
        '</g>'
    }
  ];

  global.BAMBU_DIM_SUM = {
    // Stated odds; the runtime draws once per launch and never re-rolls.
    chance: 0.01,
    dishes: dishes
  };
})(typeof window !== 'undefined' ? window : globalThis);
