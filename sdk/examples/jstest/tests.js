// Conformance checks for the JavaScript engine running on QAR-OS. Expected values were verified against V8 (Node.js).
let pass = 0, fail = 0;
function eq(name, got, want) {
  const g = JSON.stringify(got), w = JSON.stringify(want);
  if (g === w) pass++; else { fail++; print("JS FAIL " + name + ": got " + g + " want " + w); }
}
eq("arith", 1 + 2 * 3, 7);
eq("float-sum", 0.1 + 0.2, 0.30000000000000004);
eq("toFixed", (1.005).toFixed(2), "1.00");
eq("toString2", (255).toString(2), "11111111");
eq("toString16-frac", (0.5).toString(16), "0.8");
eq("toPrecision", (123.456).toPrecision(4), "123.5");
eq("toExponential", (12345).toExponential(2), "1.23e+4");
eq("parseFloat", parseFloat("3.14abc"), 3.14);
eq("number-print", [1e21, 1e-7, 123456789012345680000, -0, 5e-324].map(String), ["1e+21", "1e-7", "123456789012345680000", "0", "5e-324"]);
eq("Math.exp", Math.exp(1), 2.718281828459045);
eq("Math.sin", Math.sin(1), 0.8414709848078965);
eq("Math.log", Math.log(10), 2.302585092994046);
eq("Math.pow", 2 ** 0.5, 1.4142135623730951);
eq("Math.atan2", Math.atan2(1, 2), 0.4636476090008061);
eq("Math.hypot", Math.hypot(3, 4), 5);
eq("bigint", (2n ** 100n).toString(), "1267650600228229401496703205376");
eq("bigint-div", (10n ** 30n / 7n).toString(), "142857142857142857142857142857");
eq("closure", (() => { let c = 0; const f = () => ++c; f(); f(); return f(); })(), 3);
eq("class", (() => { class A { #x = 1; get x() { return this.#x; } static s() { return 5; } } class B extends A { constructor() { super(); this.y = 2; } } const b = new B(); return [b.x, b.y, A.s()]; })(), [1, 2, 5]);
eq("destructure", (() => { const { a, b: [c, d = 4], ...r } = { a: 1, b: [3], e: 5, f: 6 }; return [a, c, d, r]; })(), [1, 3, 4, { e: 5, f: 6 }]);
eq("spread", Math.max(...[1, 9, 3]), 9);
eq("generator", [...(function* () { yield 1; yield* [2, 3]; })()], [1, 2, 3]);
eq("map-set", (() => { const m = new Map([[1, "a"], [2, "b"]]); const s = new Set([1, 1, 2]); return [m.get(2), s.size, [...m.keys()]]; })(), ["b", 2, [1, 2]]);
eq("weakmap", (() => { const k = {}; const w = new WeakMap([[k, 7]]); return w.get(k); })(), 7);
eq("json", JSON.parse(JSON.stringify({ a: [1, { b: null }], c: "x\n\u00e9" })), { a: [1, { b: null }], c: "x\n\u00e9" });
eq("json-pretty", JSON.stringify({ a: [1, 2] }, null, 2), '{\n  "a": [\n    1,\n    2\n  ]\n}');
eq("regex-named", "2026-10-31".match(/(?<y>\d{4})-(?<m>\d\d)/).groups.y, "2026");
eq("regex-replace", "a1b22c333".replace(/\d+/g, m => "<" + m.length + ">"), "a<1>b<2>c<3>");
eq("regex-unicode", /^\p{L}+$/u.test("héllo"), true);
eq("regex-lookbehind", "$10 €20".match(/(?<=\$)\d+/)[0], "10");
eq("unicode-str", ["héllo".length, "😀".length, [..."😀"].length, "ß".toUpperCase(), "İ".toLowerCase().length], [5, 2, 1, "SS", 2]);
eq("normalize", "e\u0301".normalize("NFC").length, 1);
eq("date-utc", new Date(Date.UTC(2020, 1, 29, 12, 30, 15)).toISOString(), "2020-02-29T12:30:15.000Z");
eq("date-parse", Date.parse("2000-01-01T00:00:00Z"), 946684800000);
eq("date-now", Date.now() > 1.7e12, true);
eq("typedarray", (() => { const f = new Float64Array([1.5, 2.5]); const u = new Uint8Array(f.buffer); return [f[1], u[7], u.length]; })(), [2.5, 63, 16]);
eq("dataview", (() => { const d = new DataView(new ArrayBuffer(8)); d.setFloat32(0, 1.5, true); d.setInt16(4, -2); return [d.getFloat32(0, true), d.getInt16(4), d.getUint8(4)]; })(), [1.5, -2, 255]);
eq("sort", [5, 1, 10, 2].sort((a, b) => a - b), [1, 2, 5, 10]);
eq("sort-default", [5, 1, 10, 2].sort(), [1, 10, 2, 5]);
eq("proxy", (() => { const p = new Proxy({}, { get: (t, k) => k.toString() + "!" }); return p.foo; })(), "foo!");
eq("reflect", Reflect.ownKeys({ a: 1, [Symbol.for("s")]: 2 }).length, 2);
eq("symbol", (() => { const s = Symbol("x"); const o = { [s]: 1 }; return [typeof s, o[s], s.description]; })(), ["symbol", 1, "x"]);
eq("try-catch", (() => { try { null.x; } catch (e) { return e instanceof TypeError; } })(), true);
eq("finally", (() => { let log = []; try { try { throw 1; } finally { log.push("f"); } } catch (e) { log.push("c" + e); } return log; })(), ["f", "c1"]);
eq("eval", eval("1+1"), 2);
eq("Function", new Function("a", "b", "return a*b")(6, 7), 42);
eq("label", (() => { let n = 0; o: for (let i = 0; i < 5; i++) { for (let j = 0; j < 5; j++) { if (j == 3) continue o; if (i == 3) break o; n++; } } return n; })(), 9);
eq("getter-setter", (() => { const o = { _v: 1, get v() { return this._v * 2; }, set v(x) { this._v = x; } }; o.v = 21; return o.v; })(), 42);
eq("optional-chain", [({ a: { b: 1 } }).a?.b, ({}).a?.b, ({}).a?.b ?? "d"], [1, null, "d"]);
eq("array-methods", [[3, 1, 2].toSorted(), [1, 2, 3, 4].findLast(x => x % 2), [[1, [2]], 3].flat(2), Object.entries({ a: 1 })], [[1, 2, 3], 3, [1, 2, 3], [["a", 1]]]);
eq("string-methods", ["abc".padStart(5, "-"), "a,b".split(","), " x ".trim(), "abc".at(-1), "aXbX".replaceAll("X", "_")], ["--abc", ["a", "b"], "x", "c", "a_b_"]);
eq("template", ((n) => `n=${n + 1} ${[1, 2].map(x => x * 2)}`)(4), "n=5 2,4");
eq("tagged-template", ((s, ...v) => s.raw.join("|") + v.join(","))`a${1}b${2}c`, "a|b|c1,2");
eq("intl-free-locale", (1234.5).toLocaleString().length > 0, true);
eq("structuredClone-free", JSON.parse(JSON.stringify([new Date(0)]))[0], "1970-01-01T00:00:00.000Z");
eq("stack-overflow-caught", (() => { try { (function f() { f(); })(); } catch (e) { return e instanceof Error; } })(), true);
eq("deep-recursion-ok", (function r(n) { return n ? 1 + r(n - 1) : 0; })(300), 300);
eq("gc-churn", (() => { let a = []; for (let i = 0; i < 100000; i++) { a.push({ i, s: "x" + i }); if (a.length > 1000) a = []; } return a.length; })(), 901);
eq("string-concat-big", (() => { let s = ""; for (let i = 0; i < 20000; i++) s += "ab"; return s.length; })(), 40000);
// microtask / timer ordering must match browsers
const order = [];
Promise.resolve().then(() => order.push("p1"));
queueMicrotask(() => order.push("m1"));
(async () => { order.push("a0"); await null; order.push("a1"); })();
order.push("sync");
setTimeout(() => order.push("t20"), 20);
setTimeout(() => order.push("t10"), 10);
let ticks = 0; const iv = setInterval(() => { if (++ticks === 3) clearInterval(iv); }, 5);
setTimeout(() => {
  eq("event-order", order, ["a0", "sync", "p1", "m1", "a1", "t10", "t20"]);
  eq("setInterval", ticks, 3);
  const t0 = performance.now(); let x = 0; for (let i = 0; i < 300000; i++) x += Math.sqrt(i); const dt = performance.now() - t0;
  eq("loop-result", Math.round(x), 109544237);
  (async () => {
    const r = await Promise.all([1, Promise.resolve(2), new Promise(res => setTimeout(() => res(3), 5))]);
    eq("promise-all", r, [1, 2, 3]);
    try { await Promise.reject(new RangeError("boom")); } catch (e) { eq("await-reject", e.message, "boom"); }
    print("JS perf: 300k sqrt loop " + dt.toFixed(0) + " ms");
    print("JSTEST DONE pass=" + pass + " fail=" + fail);
  })();
}, 40);
