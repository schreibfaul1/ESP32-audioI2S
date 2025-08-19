# Synchronised lyrics/text

Synchronized Lyrics or also called 'Lyrics3' is a format for storing lyrics in the ID3 metadata of audio files such as MP3s. The synchronized texts are saved in a special ID3 tag called **SYLT**.

Unlike plain lyrics, synced lyrics can contain not only the lyrics of the song, but also information about when each section of lyrics should be displayed. This information is stored in the form of timestamps that indicate when each piece of text should begin and end.
Once the synced lyrics are stored in the ID3 metadata, they can be viewed by a compatible audio player. The audio player reads the SYLT tags and displays the lyrics of the song in real time while the song is playing. This allows the listener to follow the lyrics of the song in real time and sing along.

@moononournation had the idea of ​​reading the SYLT tag with the audioI2S library, and he also wrote the necessary source code.

The example shown here reads an mp3 file containing the SYLT tag, plays the mp3 file and displays the lyrics according to the timestamps in the serial terminal.

# Create your own ID3 tags for synchronised lyrics
A selection an Lyrics with Timestamp you can find here: https://www.lyricsify.com/
Open an ID3 Tag Editor, like KID3:
* choose Tag 2
* choose "Synchronised Lyrics" snd "Edit"
* copy the Lyrics from Lyricsify to the clipboard
* paste "From Clipboard"

<img width="1081" height="615" alt="image" src="https://github.com/user-attachments/assets/d15fe7cc-c48a-41dd-9c1a-52cfec9621d9" />
