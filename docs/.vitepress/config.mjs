import { defineConfig } from 'vitepress'

export default defineConfig({
  title: "Alkyl",
  description: "A fast, compiled, statically-typed systems programming language.",
  
  // Base URL for GitHub Pages
  // base: '/alkyl/', 
  
  themeConfig: {
    logo: '/logo.png', // Assuming you have a logo.png in public folder
    nav: [
      { text: 'Home', link: '/' },
      { text: 'Guide', link: '/guide/hello-world' }
    ],

    sidebar: [
      {
        text: 'Getting Started',
        items: [
          { text: 'Hello World', link: '/guide/hello-world' },
          { text: 'Basic Data Types', link: '/guide/data-types' }
        ]
      }
    ],

    socialLinks: [
      { icon: 'github', link: 'https://github.com/faranaiki/alkyl' }
    ],

    search: {
      provider: 'local'
    }
  },
  
  appearance: 'dark' // Default to dark mode for a premium look
})
