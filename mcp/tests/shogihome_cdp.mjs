// Optional interoperability test adapter for a separately launched ShogiHome.
// The real client's Electron IPC API performs all CSA communication.
let input = "";
for await (const chunk of process.stdin) input += chunk;
const { endpoint, expression } = JSON.parse(input);
const pages = await (await fetch(`${endpoint}/json`)).json();
const page = pages.find(p => p.type === "page");
if (!page) throw new Error("No ShogiHome renderer found");
const socket = new WebSocket(page.webSocketDebuggerUrl);
const timeout = setTimeout(() => { socket.close(); process.exit(1); }, 10000);
socket.addEventListener("open", () => {
  socket.send(JSON.stringify({ id: 1, method: "Runtime.evaluate", params: {
    expression, returnByValue: true, awaitPromise: true,
  } }));
});
socket.addEventListener("message", event => {
  const response = JSON.parse(event.data);
  if (response.id !== 1) return;
  clearTimeout(timeout);
  socket.close();
  if (response.error || response.result.exceptionDetails) {
    console.error(JSON.stringify(response));
    process.exitCode = 1;
  } else {
    console.log(JSON.stringify(response.result.result.value ?? null));
  }
});
