import { useState } from "react";
import { cn } from "@/lib/utils";

const keys = [
  ["C", "±", "%", "÷"],
  ["7", "8", "9", "×"],
  ["4", "5", "6", "−"],
  ["1", "2", "3", "+"],
  ["0", ".", "="],
];

export function CalculatorApp() {
  const [display, setDisplay] = useState("0");
  const [prev, setPrev] = useState<number | null>(null);
  const [op, setOp] = useState<string | null>(null);
  const [fresh, setFresh] = useState(true);

  function inputDigit(d: string) {
    if (fresh || display === "0") {
      setDisplay(d === "." ? "0." : d);
      setFresh(false);
    } else if (d === "." && display.includes(".")) {
      // ignore
    } else {
      setDisplay(display + d);
    }
  }

  function compute(a: number, b: number, o: string) {
    switch (o) {
      case "+":
        return a + b;
      case "−":
        return a - b;
      case "×":
        return a * b;
      case "÷":
        return b === 0 ? NaN : a / b;
      default:
        return b;
    }
  }

  function setOperation(o: string) {
    const current = parseFloat(display);
    if (prev !== null && op && !fresh) {
      const result = compute(prev, current, op);
      setPrev(result);
      setDisplay(formatNum(result));
    } else {
      setPrev(current);
    }
    setOp(o);
    setFresh(true);
  }

  function equals() {
    if (op !== null && prev !== null) {
      const result = compute(prev, parseFloat(display), op);
      setDisplay(formatNum(result));
      setPrev(null);
      setOp(null);
      setFresh(true);
    }
  }

  function formatNum(n: number) {
    if (!isFinite(n)) return "Error";
    return parseFloat(n.toPrecision(12)).toString();
  }

  function press(k: string) {
    if (k === "C") {
      setDisplay("0");
      setPrev(null);
      setOp(null);
      setFresh(true);
    } else if (k === "±") {
      setDisplay((d) => (parseFloat(d) * -1).toString());
    } else if (k === "%") {
      setDisplay((d) => formatNum(parseFloat(d) / 100));
    } else if (["+", "−", "×", "÷"].includes(k)) {
      setOperation(k);
    } else if (k === "=") {
      equals();
    } else {
      inputDigit(k);
    }
  }

  return (
    <div className="flex h-full flex-col bg-card p-4">
      <div className="mb-3 flex flex-1 items-end justify-end overflow-hidden rounded-xl bg-secondary/60 px-5 py-4">
        <span className="truncate text-5xl font-light tabular-nums">{display}</span>
      </div>
      <div className="grid grid-cols-4 gap-2">
        {keys.flat().map((k) => {
          const isOp = ["+", "−", "×", "÷", "="].includes(k);
          const isFn = ["C", "±", "%"].includes(k);
          return (
            <button
              key={k}
              onClick={() => press(k)}
              className={cn(
                "grid h-14 place-items-center rounded-xl text-xl font-medium transition-colors active:scale-95",
                k === "0" && "col-span-2",
                isOp && "bg-primary text-primary-foreground hover:opacity-90",
                isFn && "bg-muted hover:bg-muted/70",
                !isOp && !isFn && "bg-secondary hover:bg-secondary/70",
                op === k && "ring-2 ring-ring",
              )}
            >
              {k}
            </button>
          );
        })}
      </div>
    </div>
  );
}
