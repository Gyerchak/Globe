import discord

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

tokens = load_tokens('tokens.txt')
TOKEN = tokens['Discord:Client']      # <-- choose the correct label
# -----------------------------------

intents = discord.Intents.default()
intents.message_content = True

client = discord.Client(intents=intents)

@client.event
async def on_ready():
    print(f'Zalogowano jako {client.user}')

@client.event
async def on_message(message):
    if message.author == client.user:
        return
    if message.content.startswith('!ping'):
        await message.channel.send('Pong!')

client.run(TOKEN)