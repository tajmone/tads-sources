for %%i in (*.htm) do (
	sed -r "s/<!-- +AHref=""([^""]+)"" +-->/<a1 href=""\1"">/g;s/<!-- +\/AHref +-->/<\/a1>/g" %%i > xxx\%%i
)
