import discord
import asyncio
import json
import os
import websockets

# ---------- config ----------
BUILD_CHANNEL = 'build'
PLAN_CHANNEL = 'plan'
OUTPUT_CHANNEL = 'thinking'
WS_URL = 'ws://localhost:8765'
MODE_FILE = os.path.join(os.path.dirname(__file__), 'mode_state.txt')

# ---------- token loading ----------
def load_tokens(filepath):
    tokens = {}
    with open(filepath, 'r') as f:
        lines = [line.strip() for line in f if line.strip()]
        for i in range(0, len(lines), 2):
            label = lines[i].rstrip(':')
            token = lines[i+1]
            tokens[label] = token
    return tokens

tokens = load_tokens(os.path.join(os.path.dirname(__file__), 'tokens.txt'))
TOKEN = tokens['Discord:Client']

# ---------- mode file ----------
def read_mode():
    try:
        with open(MODE_FILE, 'r') as f:
            return f.read().strip().lower()
    except FileNotFoundError:
        return 'plan'

def write_mode(mode):
    with open(MODE_FILE, 'w') as f:
        f.write(mode)

# ---------- websocket ----------
class RemoteClient:
    def __init__(self):
        self.ws = None
        self.bot = None
        self._lock = asyncio.Lock()

    async def connect(self):
        try:
            self.ws = await websockets.connect(WS_URL, ping_interval=30, ping_timeout=10)
            print(f'Connected to VS Code extension at {WS_URL}')
            asyncio.create_task(self._listen())
            return True
        except Exception as e:
            print(f'WebSocket connection failed: {e}')
            self.ws = None
            return False

    async def _listen(self):
        while self.ws:
            try:
                message = await self.ws.recv()
                data = json.loads(message)
                await self._handle_response(data)
            except websockets.exceptions.ConnectionClosed:
                print('WebSocket disconnected')
                self.ws = None
                break
            except Exception as e:
                print(f'WebSocket listen error: {e}')
                break

    async def _handle_response(self, data):
        if not self.bot:
            return
        msg_type = data.get('type', '')
        text = data.get('text', '')
        if msg_type in ('output', 'status', 'error'):
            channel = discord.utils.get(self.bot.get_all_channels(), name=OUTPUT_CHANNEL)
            if channel:
                prefix = {'output': '```\n', 'status': '', 'error': '⚠️ '}.get(msg_type, '')
                suffix = {'output': '\n```'}.get(msg_type, '')
                await channel.send(f'{prefix}{text}{suffix}')

    async def send(self, data):
        async with self._lock:
            if not self.ws:
                return False
            try:
                await self.ws.send(json.dumps(data))
                return True
            except Exception as e:
                print(f'WebSocket send error: {e}')
                self.ws = None
                return False

    async def ensure_connected(self):
        if not self.ws:
            return await self.connect()
        return True

    async def close(self):
        if self.ws:
            await self.ws.close()
            self.ws = None

remote = RemoteClient()

# ---------- discord bot ----------
intents = discord.Intents.default()
intents.message_content = True
intents.guilds = True

client = discord.Client(intents=intents)

@client.event
async def on_ready():
    print(f'Logged in as {client.user}')
    remote.bot = client
    await remote.connect()

@client.event
async def on_message(message):
    if message.author == client.user:
        return

    if message.content.startswith('!'):
        return

    channel_name = message.channel.name.lower() if message.channel else ''
    content = message.content.strip()

    if not content or channel_name not in (BUILD_CHANNEL, PLAN_CHANNEL):
        return

    if not await remote.ensure_connected():
        await message.add_reaction('⚠️')
        return

    target_mode = 'build' if channel_name == BUILD_CHANNEL else 'plan'
    current_mode = read_mode()

    if current_mode != target_mode:
        await remote.send({'type': 'tab'})
        write_mode(target_mode)
        await asyncio.sleep(0.5)

    await remote.send({'type': target_mode, 'text': content})

@client.event
async def on_error(event, *args, **kwargs):
    print(f'Discord error: {event}')

client.run(TOKEN)
