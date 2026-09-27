/** Regex-capable search field with builder popover. @startingPoint section="Fields" subtitle="40px search pill with regex toggle + builder" viewport="420x64" */
export interface SearchFieldProps {
  placeholder?: string;
  /** Fires on every input / builder change with the current query string */
  onQuery?: (query: string) => void;
  style?: React.CSSProperties;
}
