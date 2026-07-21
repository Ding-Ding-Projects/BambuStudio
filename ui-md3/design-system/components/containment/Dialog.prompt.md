Modal shell for Export / Send-to-print / Add-filament.

```jsx
<Dialog icon="print" title="Print Plate 1" subtitle="Send to printer over the network" onClose={close}
  footer={<><Button variant="text">Cancel</Button><Button variant="filled" size="lg" icon="send">Send print</Button></>}>
  …content…
</Dialog>
```

Position: parent must be the window root (position:relative/absolute).
