# MancsrvGame
This is a revision of the Mancala game.

This prgoram basically used socket in C, establishing a Mancala server and allowing mutiple players to connect to the server.

Usually, Mancala is played with pebbles and a two-player board or four-player board. This program will allow any number of players.

We implemented our version of mancala and changed some rules as follows:

Each player has a side of the board. There are six depressions per side plus a larger depression at each end. The larger depression at the right end of your side of the board is your "end" pit.

Each player begins with four pebbles in each regular pit, and an empty end pit.

On your turn, you choose any non-empty pit on your side of the board(not including the end pit), and pick up all of the peebles from that pit and distribute them to the right: one pebble in the next pit to the right, another pebble into the next pit to the right after that; and so on until you've distributed all of them. You might manage to put a pebble into your end pit. If you go beyond your end pit, that's fine, you put pebbles into other people's pits. However, you always skip other people's end pits.

If you end your turn by putting a pebble into your own pit, then you get another turn. There is no limit to how many consecutive turns you can get by this method.

After that, it's another player's turn. The game ends when any one player's side is empty. At the end of the game, each player's score is all of the pebbles remaining on their side(which will consist mostly of the end pit, and will consist exclusively of the end pit for the player who emptied their side).

