Segmented control — process tabs (Quality/Strength/Support/Others), speed modes, theme/density toggles.

```jsx
<SegmentedControl size="md" value={theme} onChange={setTheme}
  options={[{id:'light',label:'Light',icon:'light_mode'},{id:'dark',label:'Dark',icon:'dark_mode'}]} />
```

Selected item = filled primary; track = sc-highest.
