const vscode = require('vscode');
const WebSocket = require('ws');
const { exec } = require('child_process');
const path = require('path');
const fs = require('fs');

let wss = null;
let outputChannel = null;

const MODE_FILE = path.join(__dirname, '..', 'mode_state.txt');
const PORT = 8765;

function findOpenCode() {
    const home = process.env.USERPROFILE || process.env.HOME;
    const candidates = [
        path.join(home || '', '.opencode', 'bin', 'opencode.exe'),
        path.join(home || '', '.opencode', 'bin', 'opencode'),
        'opencode'
    ];
    for (const p of candidates) {
        try {
            if (fs.existsSync(p) || p === 'opencode') return p;
        } catch {}
    }
    return 'opencode';
}
const OPENCODE_CMD = findOpenCode();

function log(msg) {
    const line = `[${new Date().toISOString()}] ${msg}`;
    console.log(line);
    if (outputChannel) outputChannel.appendLine(line);
}

function readMode() {
    try {
        return fs.readFileSync(MODE_FILE, 'utf8').trim().toLowerCase();
    } catch {
        return 'plan';
    }
}

function writeMode(mode) {
    fs.writeFileSync(MODE_FILE, mode, 'utf8');
}

function sendResponse(ws, type, text) {
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ type, text }));
    }
}

function getActiveTerminal() {
    let terminal = vscode.window.activeTerminal;
    if (!terminal) {
        terminal = vscode.window.createTerminal('opencode-remote');
        terminal.show();
    }
    return terminal;
}

function executeCommand(command) {
    return new Promise((resolve) => {
        exec(command, {
            cwd: vscode.workspace.workspaceFolders?.[0]?.uri?.fsPath || process.cwd(),
            timeout: 120000
        }, (error, stdout, stderr) => {
            const output = stdout || '';
            const err = stderr || '';
            if (error && !stdout) {
                resolve(`ERROR: ${error.message}\n${err}`);
            } else {
                resolve(output + (err ? `\nSTDERR:\n${err}` : ''));
            }
        });
    });
}

function runOpenCode(prompt) {
    return new Promise((resolve) => {
        const cmd = `"${OPENCODE_CMD}" run "${prompt.replace(/"/g, '\\"')}"`;
        exec(cmd, {
            cwd: vscode.workspace.workspaceFolders?.[0]?.uri?.fsPath || process.cwd(),
            timeout: 300000
        }, (error, stdout, stderr) => {
            if (error) {
                resolve(`OpenCode Error: ${error.message}\n${stderr}`);
            } else {
                resolve(stdout || '(no output)');
            }
        });
    });
}

async function handlePlan(ws, text) {
    const display = text;
    sendResponse(ws, 'status', `Running: ${display}`);

    const terminal = getActiveTerminal();
    terminal.show();
    terminal.sendText(display);

    const output = await executeCommand(text);
    sendResponse(ws, 'output', output);
    return output;
}

async function handleBuild(ws, text) {
    sendResponse(ws, 'status', `OpenCode: ${text}`);

    const terminal = getActiveTerminal();
    terminal.show();

    const opencodeCmd = `opencode run "${text.replace(/"/g, '\\"')}"`;
    terminal.sendText(opencodeCmd);

    const output = await runOpenCode(text);
    sendResponse(ws, 'output', output);
    return output;
}

function handleTab(ws) {
    const terminal = getActiveTerminal();
    if (terminal) {
        terminal.show();
        terminal.sendText('\t');
    }
    const current = readMode();
    const newMode = current === 'build' ? 'plan' : 'build';
    writeMode(newMode);
    sendResponse(ws, 'status', `Switched mode to: ${newMode}`);
}

function startServer() {
    if (wss) {
        log('Server already running');
        return;
    }

    wss = new WebSocket.Server({ port: PORT }, () => {
        log(`OpenCode Remote Control server started on ws://localhost:${PORT}`);
        vscode.window.showInformationMessage(`OpenCode Remote: ws://localhost:${PORT}`);
    });

    wss.on('connection', (ws) => {
        log('Client connected');
        sendResponse(ws, 'status', `Connected | Mode: ${readMode()}`);

        ws.on('message', async (data) => {
            try {
                const msg = JSON.parse(data.toString());

                switch (msg.type) {
                    case 'plan':
                        await handlePlan(ws, msg.text);
                        break;
                    case 'build':
                        await handleBuild(ws, msg.text);
                        break;
                    case 'tab':
                        handleTab(ws);
                        break;
                    case 'ping':
                        sendResponse(ws, 'pong', 'ok');
                        break;
                    default:
                        sendResponse(ws, 'error', `Unknown command type: ${msg.type}`);
                }
            } catch (err) {
                log(`Error handling message: ${err.message}`);
                sendResponse(ws, 'error', err.message);
            }
        });

        ws.on('close', () => log('Client disconnected'));
        ws.on('error', (err) => log(`WebSocket error: ${err.message}`));
    });

    wss.on('error', (err) => {
        log(`Server error: ${err.message}`);
        vscode.window.showErrorMessage(`OpenCode Remote error: ${err.message}`);
    });
}

function stopServer() {
    if (wss) {
        wss.close(() => log('Server stopped'));
        wss = null;
    }
}

function activate(context) {
    outputChannel = vscode.window.createOutputChannel('OpenCode Remote');
    log('Extension activated');

    context.subscriptions.push(
        vscode.commands.registerCommand('opencode-remote.start', startServer),
        vscode.commands.registerCommand('opencode-remote.stop', stopServer),
        vscode.commands.registerCommand('opencode-remote.status', () => {
            const mode = readMode();
            const running = wss ? 'Running' : 'Stopped';
            vscode.window.showInformationMessage(
                `OpenCode Remote: ${running} | Mode: ${mode} | Port: ${PORT}`
            );
        })
    );

    startServer();

    context.subscriptions.push({ dispose: stopServer });
}

function deactivate() {
    stopServer();
}

module.exports = { activate, deactivate };
