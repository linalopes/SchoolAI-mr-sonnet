const express = require('express')
const app = express()

app.use(express.json())
app.use(express.static('.'))

const ANTHROPIC_API_URL = 'https://api.anthropic.com/v1/messages'
const ANTHROPIC_KEY = (process.env.ANTHROPIC_API_KEY || process.env.ANTHROPIC_KEY || '').trim().replace(/^"(.*)"$/, '$1')

app.post('/api/poem', async (req, res) => {
  if (!ANTHROPIC_KEY) {
    return res.status(400).json({
      error: {
        message: 'Missing ANTHROPIC_API_KEY. Set it before starting the server.'
      }
    })
  }

  try {
    const response = await fetch(ANTHROPIC_API_URL, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'x-api-key': ANTHROPIC_KEY,
        'anthropic-version': '2023-06-01'
      },
      body: JSON.stringify(req.body),
      signal: AbortSignal.timeout(30000)
    })

    const raw = await response.text()
    let data
    try {
      data = raw ? JSON.parse(raw) : {}
    } catch {
      data = { error: { message: 'Invalid response from Anthropic API.' } }
    }

    if (!response.ok) {
      return res.status(response.status).json(data)
    }

    return res.json(data)
  } catch (err) {
    if (err.name === 'TimeoutError' || err.name === 'AbortError') {
      return res.status(504).json({
        error: { message: 'Anthropic API timed out. Please try again.' }
      })
    }

    return res.status(502).json({
      error: { message: `Cannot reach Anthropic API: ${err.message}` }
    })
  }
})

app.listen(3000, () => {
  console.log('────────────────────────────────────')
  console.log('  Mr. Sonnet → http://localhost:3000')
  console.log('────────────────────────────────────')
})
