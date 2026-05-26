import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';

// https://astro.build/config
export default defineConfig({
  site: "https://doritothepug.github.io/Hamster",
  base: "Hamster",
	integrations: [
		starlight({
			title: 'Hamster',
			logo: {
				src: './src/assets/hamster-logo.png',
			},
			favicon: '/hamster-logo.png',
			social: {
				github: 'https://github.com/DoritoThePug/Hamster',
			},
			sidebar: [
				{
					label: 'Start Here',
					items: [
						{ label: 'Getting Started', link: 'start-here/getting-started' },
						{ label: 'Your First Game', link: 'start-here/your-first-game' },
					],
				},
				{
					label: 'Guides',
					autogenerate: { directory: 'guides' },
				},
				{
					label: 'Reference',
					autogenerate: { directory: 'reference' },
				},
			],
		}),
	],
});
