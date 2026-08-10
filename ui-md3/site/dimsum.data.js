/*
 * Bounded dim-sum metadata cache for the startup surprise.
 *
 * The authoritative names and image metadata come from the public catalog at
 * https://raw.githubusercontent.com/Ding-Ding-Projects/dim-sum-photos/main/catalog/index.json
 * revision f77ea1169db0bfc17365414c44ff495a823c6823. The cache is deliberately
 * small so the static site does not download the eight-megabyte catalog during
 * startup. Photos are not copied into this repository: every URL below is an
 * published `catalog-v1` release asset from the catalog repository. GitHub
 * currently reports that release as mutable, so the online catalog contract
 * revalidates every cached record and asset digest rather than claiming the tag
 * alone is an integrity boundary.
 */
(function (global) {
  'use strict';

  var SOURCE_REVISION = 'f77ea1169db0bfc17365414c44ff495a823c6823';
  var CATALOG_URL =
    'https://raw.githubusercontent.com/Ding-Ding-Projects/dim-sum-photos/main/catalog/index.json';
  var RELEASE_ROOT =
    'https://github.com/Ding-Ding-Projects/dim-sum-photos/releases/download/catalog-v1/';

  var dishes = [
    ['hk-dish-0001', 'Classic Har Gow', '蝦餃',
      'Warm tea-house photograph of Classic Har Gow', '港式茶樓木枱上嘅蝦餃',
      'hk-dish-0001-classic-har-gow.png',
      'c6ff2d32938f1e4c4ea685442f69227b8cd387f302ab8f8a62e8dd96c62b5ac0'],
    ['hk-dish-0011', 'Classic Siu Mai', '燒賣',
      'Warm tea-house photograph of Classic Siu Mai', '港式茶樓木枱上嘅燒賣',
      'hk-dish-0011-classic-siu-mai.png',
      '2afac75530d1063c324b1e63904ff860b5d6346d8e49f8aa4eb1df8f3527d3bb'],
    ['hk-dish-0051', 'Classic Char Siu Bao', '叉燒包',
      'Warm tea-house photograph of Classic Char Siu Bao', '港式茶樓木枱上嘅叉燒包',
      'hk-dish-0051-classic-char-siu-bao.png',
      '55d8b94fb2712175f5cbca6ba50ce6e27be37e9c48114c0fc8aed8e4dc541459'],
    ['hk-dish-0152', 'Shrimp Rice Noodle Rolls', '鮮蝦腸粉',
      'Warm tea-house photograph of Shrimp Rice Noodle Rolls', '港式茶樓木枱上嘅鮮蝦腸粉',
      'hk-dish-0152-shrimp-rice-noodle-rolls.png',
      'afcb2326378005ab80c7fe6e26a584c07058526ee9f854e1c8ee0ae799df5068'],
    ['hk-dish-0139', 'Puff Pastry Egg Tarts', '酥皮蛋撻',
      'Warm tea-house photograph of Puff Pastry Egg Tarts', '港式茶樓木枱上嘅酥皮蛋撻',
      'hk-dish-0139-puff-pastry-egg-tarts.png',
      '3f5576aaf8e0ea41feac26317c2d5ceab8e89bb854ac940652e88743db42484c'],
    ['hk-dish-0193', 'Lotus Leaf Sticky Rice with Chicken', '荷葉糯米雞',
      'Warm tea-house photograph of Lotus Leaf Sticky Rice with Chicken', '港式茶樓木枱上嘅荷葉糯米雞',
      'hk-dish-0193-lotus-leaf-chicken-rice.png',
      'f50bdac5957641eb8e9efed8cd0c9c3e4776276aa212e9ffc5d47297964e74bf'],
    ['hk-dish-0651', 'Hong Kong Festive Pan-Fried Turnip Cake', '香港應節香煎蘿蔔糕',
      'A single serving of Hong Kong Festive Pan-Fried Turnip Cake, rectangular turnip-cake slices pan-fried golden with visible sausage and shrimp flecks.',
      '一份喺香港農曆新年家庭聚會嘅紅色佈置餐枱上擺好嘅香港應節香煎蘿蔔糕。',
      'hk-dish-0651-hong-kong-festive-pan-fried-turnip-cake.png',
      'e4579e0aa925e6118246e8f003ec09768f2a833bf04c0f3effdac31f3fdbc812'],
    ['hk-dish-0509', 'Classic Egg Waffles', '原味雞蛋仔',
      'A single serving of Classic Egg Waffles, a freshly baked bubble waffle with crisp round shells and a tender centre.',
      '一份擺喺香港雞蛋仔小檔嘅原味雞蛋仔。',
      'hk-dish-0509-classic-egg-waffles.png',
      'b357afabb12e0cd4cce7cab1d2e3a5f20665aa043b192de4a2a33b0018fa8887'],
    ['hk-dish-0027', 'Steamed Chicken Feet in Black Bean Sauce', '豉汁蒸鳳爪',
      'Warm tea-house photograph of Steamed Chicken Feet in Black Bean Sauce',
      '港式茶樓木枱上嘅豉汁蒸鳳爪',
      'hk-dish-0027-black-bean-chicken-feet.png',
      'efb9ee97ba6ce9cae7b8d831988d8948e6927a61369acdf4792074e396a65a91'],
    ['hk-dish-0231', 'Mango Pomelo Sago', '楊枝甘露',
      'Warm tea-house photograph of Mango Pomelo Sago', '港式茶樓木枱上嘅楊枝甘露',
      'hk-dish-0231-mango-pomelo-sago.png',
      'a1f6f3a266b1d2b58837e5fb2b1c42869ba63067af5eda9eb9deb53240df1a8d']
  ].map(function (dish) {
    return {
      id: dish[0],
      en: dish[1],
      yue: dish[2],
      altEn: dish[3],
      altYue: dish[4],
      photo: {
        file: dish[5],
        url: RELEASE_ROOT + encodeURIComponent(dish[5]),
        sha256: dish[6]
      }
    };
  });

  global.BAMBU_DIM_SUM = {
    schemaVersion: 1,
    // One fresh draw per eligible launch. The result is never re-rolled.
    chance: 0.10,
    source: {
      catalogUrl: CATALOG_URL,
      revision: SOURCE_REVISION,
      releaseTag: 'catalog-v1'
    },
    dishes: dishes
  };
})(typeof window !== 'undefined' ? window : globalThis);
