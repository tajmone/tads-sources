#charset "us-ascii"

/*
 *   Copyright (c) 1999, 2002 by Michael J. Roberts.  Permission is granted
 *   to anyone to copy and use this file for any purpose.
 *
 *   This is a starter TADS 3 source file.  This is a complete TADS game
 *   that you can compile and run.
 *
 *   To compile this game in TADS Workbench, open the "Build" menu and
 *   select "Compile for Debugging."  To run the game, after compiling it,
 *   open the "Debug" menu and select "Go."
 *
 *   This is the "advanced" starter game - it has only the minimum set of
 *   definitions needed for a working game.  If you would like some more
 *   examples, create a new game, and choose the "introductory" version when
 *   asked for the type of starter game to create.
 */

/*
 *   Include the main header for the standard TADS 3 adventure library. Note
 *   that this does NOT include the entire source code for the library; this
 *   merely includes some definitions for our use here.  The main library
 *   must be "linked" into the finished program by including the file
 *   "adv3.tl" in the list of modules specified when compiling. In TADS
 *   Workbench, simply include adv3.tl in the "Source Files" section of the
 *   project.
 *
 *   Also include the US English definitions, since this game is written in
 *   English.
 */
#include <adv3.h>
#include <en_us.h>

/*
 *   Our game credits and version information.  This object isn't required
 *   by the system, but our GameInfo initialization above needs this for
 *   some of its information.
 *
 *   IMPORTANT - You should customize some of the text below, as marked: the
 *   name of your game, your byline, and so on.
 */
versionInfo: GameID
    IFID = '573a8b18-1008-ca66-9580-9a156f82eefa '
    name = 'The Further Adventures of Heidi'
    byline = 'by An Author'
    htmlByline = 'by <a href="mailto:whatever@nospam.org">
        ERIC EVE</a>'
    version = '3.0.16'
    authorEmail = 'ERIC EVE <whatever@nospam.org>'
    desc = 'This is an unexciting tutorial game based loosely on
        The Adventures of Heidi by Roger Firth and Sonja Kesserich.'
    htmlDesc = 'This is an unexciting tutorial game based loosely on
        <i>The Adventures of Heidi</i> by Roger Firth and Sonja Kesserich.'

    showCredit()
    {
        /* show our credits */
        "The TADS 3 language and library were created by Michael J. Roberts.<.p>
        The original <i>Adventures of Heidi</i> was a simple tutorial game for
        the Inform language written by Roger Firth and Sonja Kesserich.";

        /*
         *   The game credits are displayed first, but the library will
         *   display additional credits for library modules.  It's a good
         *   idea to show a blank line after the game credits to separate
         *   them visually from the (usually one-liner) library credits that
         *   follow.
         */
        "\b";
    }
    showAbout()
    {
        "<i>The Further Adventures of Heidi</i><.p>
        A Tutorial Game for TADS 3";
    }
;



/*
 *   Define the player character.  The name of this object is not important,
 *   but note that it has to match up with the name we use in the main()
 *   routine to initialize the game, below.
 *
 *   Note that we aren't required to define any vocabulary or description
 *   for this object, because the class Actor, defined in the library,
 *   automatically provides the appropriate definitions for an Actor when
 *   the Actor is serving as the player character.  Note also that we don't
 *   have to do anything special in this object definition to make the Actor
 *   the player character; any Actor can serve as the player character, and
 *   we'll establish this one as the PC in main(), below.
 */
me: Actor
    /* the initial location */
    location = outsideCottage
;

+ Wearable 'plain pretty blue dress' 'blue dress'
    "It's quite plain, but you think it pretty enough. "
    wornBy = me
    dobjFor(Doff)
    {
        check()
        {
            failCheck('You can\'t wander around half naked! ');
        }
    }
;

/*
 *   The only thing that HAS to be done in gameMain is to set the
 *   initialPlayerChar, which can be called anything we like, but which is
 *   normally (since it's the default) called me.
 *
 *   In practice you'll normally want to override the showIntro() method as
 *   well to display an introduction -- usually one that's rather more
 *   substantial than than shown here.
 */

gameMain: GameMainDef
    initialPlayerChar = me
    showIntro()
    {
        "Welcome to the Further Adventures of Heidi!\b";
    }
    showGoodbye()
    {
        "<.p>Thanks for playing!\b";
    }
    maxScore = 7
;

/*
 *   This exemplifies how a function may be defined in TADS 3. This one is
 *   fairly pointless, since it's only called from one point in the game. In
 *   a game that called finishGameMsg from may different places, a wrapper
 *   function like this might be more useful.
 *
 *   Note that the function keyword here is optional, and most TADS 3 code
 *   would omit it, defining the function thus:
 *
 *   endGame(flag) { finishGameMsg(flag,
 *   [finishOptionUndo,finishOptionFullScore]); }
 */

function endGame(flag)
    {
        finishGameMsg(flag, [finishOptionUndo,finishOptionFullScore]);
    }

    /* Modifications to behaviour of various classes */

    modify Actor
    makeProper
    {
        if(isProperName == nil && properName != nil)
        {
            isProperName = true;
            name = properName;

            /* Method 1: */
            local tokList = Tokenizer.tokenize(properName);
            for (local i = 1, local cnt = tokList.length() ; i <= cnt ; ++i)
            {
                if(i < cnt)
                    cmdDict.addWord(self, getTokVal(tokList[i]), &adjective);
                cmdDict.addWord(self, getTokVal(tokList[i]), &noun);
            }

            /*
             *   Simpler Alternative: this doesn't add forenames as both
             *   nouns and adjectives, but as adjectives only with the final
             *   name as a noun:
             */
            //      initializeVocabWith(properName);
        }
    }
;

/*
 *   Modifications to Thing to implement a listRemoteContents() method, for
 *   looking through the cottage window.
 */

modify Thing
    listLocation_ = nil
    listRemoteContents(otherLocation)
    {
        listLocation_ = otherLocation;
        try
        {
            lookAround(gActor, LookListSpecials | LookListPortables);
        }
        finally
        {
            listLocation_ = nil;
        }
    }

    adjustLookAroundTable(tab, pov, actor)
    {
        inherited(tab, pov, actor);
        if(listLocation_ !=  nil)
        {
            local lst = tab.keysToList();
            foreach(local cur in lst)
            {
                if(!cur.isIn(listLocation_))
                    tab.removeElement(cur);
            }
        }
    }
;


/* Definition of additional verbs */

DefineTAction(Cross);

VerbRule(Cross)
    ('cross' | 'ford' | 'wade') singleDobj
    : CrossAction
    verbPhrase = 'cross/crossing (what)'
;

modify Thing
    dobjFor(Cross)
    {
        preCond = [objVisible]
        verify() {illogical('{The dobj/he} is not something you can cross. ' ); }
    }
;

DefineTAction(Row);

VerbRule(Row)
    'row' singleDobj
    : RowAction
    verbPhrase = 'row/rowing (what)'
;

modify Thing
    dobjFor(Row)
    {
        preCond = [touchObj]
        verify() { illogical('{You/he} can\'t row {that dobj/him}'); }
    }
;

DefineTAction(Ring);

VerbRule(Ring)
    'ring' singleDobj
    : RingAction
    verbPhrase = 'ring/ring (what)'
;

modify Thing
    dobjFor(Ring)
    {
        preCond = [touchObj]
        verify() { illogical('{You/he} can\'t ring {that dobj/him}'); }
    }
;

/* Definitions of additional special classes */


class Diggable : Floor
    dobjFor(DigWith)
    {
        preCond = [objVisible, touchObj]
        verify() {}
        check() {}
        action()
        {
            "Digging here reveals nothing of interest. ";
        }
    }
    putDestMessage = &putDestFloor
;

class ForestRoom : OutdoorRoom
    atmosphereList : ShuffledEventList
    {
        [
            'A fox dashes across your path.\n',
            'A clutch of rabbits dash back among the trees.\n',
            'A deer suddenly leaps out from the trees, then darts back off into the forest.\n',
            'There is a rustling in the undergrowth.\n',
            'There is a sudden flapping of wings as a pair of birds take flight off to the left.\n'
        ]
        eventPercent = 90
        eventReduceAfter = 6
        eventReduceTo = 50
    }
;

/*
 *   This MultiInstance effectively extends the functionality of the
 *   ForestRoom class by ensuring that we provide a response to EXAMINE
 *   TREES in every room of the ForestRoom class
 */

MultiInstance
    instanceObject : Decoration { 'pine tree*trees*pines' 'pine trees'
        "The forest is full of tall, fast-growing pines, although the occasional oak,
        beach and sycamore can occasionally be seen among them. "
        isPlural = true
    }
    initialLocationClass = ForestRoom
;


modify Thing
    dobjFor(CleanWith)
    {
        verify() {}
        check() { failCheck(&cannotCleanMsg); }
    }
;


modify InstructionsAction
    customVerbs = ['ROW THE BOAT', 'CROSS STREAM','RING THE BELL' ]
;


modify playerActionMessages
    notASurfaceMsg = '{You/he} can\'t put anything on {the iobj/him}. '
;

/*
 *   Now for the actual scenario code - the definition of objects that
 *   define the rooms, things and actors in the game.
 */

/*
 *   ********************** OUTSIDE THE COTTAGE *
 *************************/

outsideCottage : OutdoorRoom 'In front of a cottage' 'the front of the cottage'
    "You stand just outside a cottage; the forest stretches east.
    A short path leads round the cottage to the northwest. "

    east = forest
    in = cottageDoor
    west asExit(in)
    northwest = cottageGarden

;


+ Enterable -> (outsideCottage.in) 'pretty little cottage/house/building' 'pretty little cottage'
    "It's just the sort of pretty little cottage that townspeople dream of living
    in, with roses round the door and a neat little window frame freshly painted in
    green. "
    cannotTakeMsg = 'It may be a small cottage, but it\'s still a lot bigger than you are;
        you can\'t walk around with it! '
    cannotCleanMsg = dobjMsg('You don\'t have time for that right now. ')
    notASurfaceMsg = iobjMsg('You can\'t reach the roof. ')
;

+ cottageDoor : LockableWithKey, Door 'door' 'door'
    "It's a neat little door, painted green to match the window frame. "
    keyList = [cottageKey]
;

+ Distant 'forest' 'forest'
    "The forest is off to the east. "
    tooDistantMsg = 'It\'s too far away for that. '
;


cottageWindow : SenseConnector, Fixture 'window' 'window'
    "The cottage window has a freshly painted green frame. The glass has been recently
    cleaned. "

    dobjFor(LookThrough)
    {
        verify() {}
        check() {}
        action()
        {
            local otherLocation;
            connectorMaterial = glass;
            if(gActor.isIn(outsideCottage))
            {
                otherLocation = insideCottage;
                "You peer through the window into the neat little room inside the cottage. ";
            }
            else
            {
                otherLocation = outsideCottage;
                "Looking out through the window you see a path leading into the forest. ";
            }
            gActor.location.listRemoteContents(otherLocation);

        }
    }
    connectorMaterial = adventium
    locationList = [outsideCottage, insideCottage]
;

/*
 *   ********************** DEEP IN THE FOREST  *
 *************************/

forest : ForestRoom 'Deep in the Forest' 'the depths of the forest'
    "Through the deep foliage you glimpse a building to the west.
    A track leads to the northeast, and a path leads south."
    west = outsideCottage
    northeast = clearing
    south = outsideCave
;

/*
 *   ********************** THE CLEARING     *
 *************************/

clearing : ForestRoom 'Clearing'
    "A tall sycamore tree stands in the middle of this clearing.
    One path winds to the southwest, and another to the north."
    southwest = forest
    //  up : NoTravelMessage { "The lowest bough is just too high for you to reach. "}
    up : TravelMessage
    {  ->topOfTree
            "By clinging on to the bough you manage to haul yourself
            up the tree. "
            canTravelerPass(traveler) {return traveler.isIn(chair); }
        explainTravelBarrier(traveler) { "The lowest bough is just out of reach. "; }
    }
    north = forestPath
;


+ tree : Fixture
    'tall sycamore tree' 'sycamore tree'
    "Standing proud in the middle of the clearing, the stout
    tree looks like it should be easy to climb. A small pile of loose twigs
    has gathered at its base. "

    dobjFor(Climb) remapTo(Up)
;

++ bough : OutOfReach, Fixture 'lowest bough' 'lowest bough'
    "The lowest bough of the tree is just a bit too high up for you
    to reach from the ground. "

    canObjReachContents(obj)
    {
        if(obj.posture == standing && obj.location == chair)
            return true;
        return inherited(obj);
    }
    cannotReachFromOutsideMsg(dest)
    {
        return 'The bough is just too far from the ground for you to reach. ';
    }
;


+ Decoration 'loose small pile/twigs' 'pile of twigs'
    "There are several small twigs here, most of them small, insubstantial,
    and frankly of no use to anyone bigger than a blue-tit <<stick.moved ?
      nil : '; but there is also one fairly long, substantial stick among them'>>.
    <<stick.discover>>"
    dobjFor(Search) asDobjFor(Examine)
;

+ stick : Hidden 'long substantial stick' 'long stick'
    "It's about two feet long, a quarter of an inch in diameter, and reasonably straight. "
    iobjFor(MoveWith)
    {
        verify() {}
        check() {}
        action()
        {
            if(gDobj==nest && !nest.moved)
                replaceAction(Take, nest);
        }
    }
;

/*
 *   ********************** THE TOP OF THE TREE  *
 *************************/


topOfTree : FloorlessRoom 'At the top of the tree' 'the top of the tree'
    "You cling precariously to the trunk, next to a firm, narrow branch. "
    down = clearing
    enteringRoom(traveler)
    {
        if(!traveler.hasSeen(self) && traveler == gPlayerChar)
            addToScore(1, 'reaching the top of the tree. ');
    }
    bottomRoom = clearing
    //      receiveDrop(obj, desc)
    //      {
    //        obj.moveInto(clearing);
    //        "\^<<obj.theName>> falls to the ground below. ";
    //      }

    roomBeforeAction()
    {
        if(gActionIs(Jump))
            failCheck('Not here - you might fall to the ground and
                hurt yourself. ');
    }

    roomAfterAction()
    {
        if(gActionIs(Yell))
            "Your shout is lost on the breeze. ";
    }

;

+ branch : Surface, Fixture 'branch' 'branch'
    "The branch looks too narrow to walk, crawl or climb along, but firm enough to support
    a reasonable weight. "
;

++ nest : Container 'bird\'s nest' 'bird\'s nest'
    "It's carefully woven from twigs and moss. "

    dobjFor(LookIn)
    {

        check()
        {
            if(!isHeldBy(gActor))
            {
                "You really need to hold the nest to take a good look at what's inside. ";
                exit;
            }
        }
        action()
        {
            if(ring.moved)
            {
                "You find nothing else of interest in the nest. ";
                exit;
            }
            ring.makePresent();
            "A closer examination of the nest reveals a diamond ring inside! ";
            addToScore(1, 'finding the ring');
        }
    }
    dobjFor(Take)
    {
        check()
        {
            if(!moved && !stick.isIn(gActor))
            {
                "The nest is too far away for you to reach. ";
                exit;
            }
        }
        action()
        {
            if(!moved)
                "Using the stick you manage to pull the nest near enough to take, which
                you promptly do. ";
            inherited;
        }
    }
    dobjFor(MoveWith)
    {
        verify()
        {
            if(isHeldBy(gActor))
                illogicalAlready('{You/he} {is} already holding it. ');
        }
        check()
        {
            if(gIobj != stick)
            {
                "{You/he} can't move the nest with that. ";
                exit;
            }
        }
    }

;

+++ ring : PresentLater, Thing 'platinum diamond ring' 'diamond ring'
    "The ring comprises a sparkling solitaire diamond set in platinum. It looks like it
    could be intended as an engagement ring. "
;

/*
 *   ********************** INSIDE THE COTTAGE  *
 *************************/

insideCottage : Room 'Inside Cottage' 'the inside of the cottage'
    "The front parlour of the little cottage looks impeccably neat.
    The door out is to the east. "
    out = cottageDoorInside
    east asExit(out)
    inRoomName(pov) { return 'inside the cottage'; }
    remoteRoomContentsLister(other)
    {
        return new CustomRoomLister('Through the window, {you/he} see{s}',
                                    ' lying on the ground.');
    }
;

+ cottageDoorInside : Lockable, Door -> cottageDoor 'door' 'door';

+ chair : Chair 'wooden chair' 'wooden chair'
    "It's a plain wooden chair. "
    initSpecialDesc = "A plain wooden chair sits in the corner. "
    remoteInitSpecialDesc(actor) { "Through the cottage window you can
        see a plain wooden chair sitting in the corner of the front room. "; }
;

/*
 *   ********************** THE FOREST PATH     *
 *************************/

forestPath : ForestRoom 'forest Path'
    "This broad path leads more or less straight north-south
    through the forest. To the north the occasional puff of
    smoke floats up above the trees. "
    south = clearing
    north = fireClearing
;

/*
 *   ********************** THE FIRE CLEARING   *
 *************************/


fireClearing : OutdoorRoom 'Clearing with Fire' 'the clearing with the fire'
    "The main feature of this large clearing a large, smouldering charcoal
    fire that periodically lets off clouds of smoke. A path leads off
    to the south, and another to the northwest. "
    south = forestPath
    northwest = pathByStream
;

+ fire : Fixture 'large smoking charcoal fire' 'fire'
    "The fire is burning slowly, turning wood into charcoal. It nevertheless
    feels quite hot even from a distance, and every now and again lets out
    billows of smoke that get blown in your direction. "
    dobjFor(Examine)
    {
        verify() {}
        action() { inherited; }
    }
    dobjFor(Smell) remapTo(Smell, smoke)
    dobjFor(Default)
    {
        verify() { illogical('The fire is best left well alone; it\'s <i>very</i>
            hot and {you/he} do{es}n\'t want to get too close.<.p>'); }
    }

;

+ smoke : Vaporous 'smoke' 'smoke'
    "The thick, grey smoke rises steadily from the fire, but gusts of wind occasionally send
    billows of it in your direction. "
    //  smellDesc = "The smoke from the fire smells acrid and makes you cough. "
;

++ Odor 'acrid smokey smell/whiff/pong' 'smell of smoke'
    sourceDesc = "The smoke from the fire smells acrid and makes you cough. "
    descWithSource = "The smoke smells strongly of charred wood."
    hereWithSource = "You catch a whiff of the smoke from the fire. "
    displaySchedule = [2, 4, 6]
;

spade : Thing 'sturdy spade' 'spade'
    @burner
    "It's a sturdy spade with a broad steel blade and a firm wooden handle. "
    iobjFor(DigWith)
    {
        verify() {}
        check() {}
    }
;

/*
 *   ********************** BANK OF THE STREAM  *
 *************************/

pathByStream : OutdoorRoom 'By a stream' 'the bank of the stream'
    "The path through the trees from the southeast comes to an end on
    the banks of a stream. Across the stream to the west you can see
    an open meadow. "
    southeast = fireClearing
    west = streamWade
;

streamWade : RoomConnector
    room1 = pathByStream
    room2 = meadow
    canTravelerPass(traveler) { return boots.isWornBy(traveler); }
    explainTravelBarrier(traveler)
    {
        "Your shoes aren't waterproof. If you wade across you'll get your feet wet
        and probably catch your death of cold. ";
    }
;
/*
 *   ********************** THE MEADOW       *
 *************************/


meadow : OutdoorRoom 'Large Meadow'
    "This large, open meadow stretches almost as far as you can see
    to north, west, and south, but is bordered by a fast-flowing stream
    to the east. "
    east = streamWade
;

+ cottageKey : Key 'small brass brassy key/object/something' 'object'
    "It's a small brass key, with a faded tag you can no longer read. "
    initSpecialDesc = "A small brass object lies in the grass. "
    remoteInitSpecialDesc(actor) {

        "There is a momentary glint of something brassy as
        the sun reflects off something lying in the meadow across the stream. "; }
    dobjFor(Take)
    {
        action()
        {
            if(!moved) addToScore(1, 'retrieving the key');
            inherited;
            name = 'small brass key';
        }
    }
;

DistanceConnector [pathByStream, meadow];

stream : MultiLoc, Fixture 'stream' 'stream'
    "The stream is not terribly deep at this point, though it's flowing
    quite rapidly towards the south. "
    locationList = [pathByStream, meadow]
    dobjFor(Cross)
    {
        verify() {}
        check() {}
        action()
        {
            replaceAction(TravelVia, streamWade);
        }
    }
;

/*
 *   ********************** OUTSIDE THE CAVE    *
 *************************/

outsideCave : OutdoorRoom 'Just Outside a Cave' 'just outside the cave'
    "The path through the trees from the north comes to an end
    just outside the mouth of a large cave to the south. Behind the cave
    rises a sheer wall of rock. "
    north = forest
    in = insideCave
    south asExit(in)
;

+ Enterable 'cave/entrance' 'cave'
    "The entrance to the cave looks quite narrow, but probably just wide
    enough for someone to squeeze through. "
    connector = insideCave
;

/*
 *   ********************** INSIDE THE COTTAGE  *
 *************************/

insideCave : DarkRoom 'Inside a large cave' 'the interior of the cave'
    "The cave is larger than its narrow entrance might lead one to expect. Even
    a tall adult could stand here quite comfortably, and the cave stretches back
    quite some way. <<caveFloor.desc>>"
    out = outsideCave
    north asExit(out)
    roomParts = [caveFloor, defaultCeiling,
        caveNorthWall, defaultSouthWall,
        caveEastWall, defaultWestWall]

;


caveFloor : Diggable 'cave floor/ground' 'cave floor'
    "The floor of the cave is quite sandy; near the centre is
    <<hasBeenDug ? 'a freshly dug hole' : 'a patch that looks as if it has
        been recently disturbed'>>. "
    hasBeenDug = nil
    dobjFor(DigWith)
    {
        check()
        {
            if(hasBeenDug)
            {
                "You've already dug a hole here. ";
                exit;
            }
        }
        action()
        {
            hasBeenDug = true;
            "You dig a small hole in the sandy floor and find a buried pair of
            old Wellington boots. ";
            hole.moveInto(self);
            addToScore(1, 'finding the boots');
        }
    }
;

hole : Container, Fixture 'hole' 'hole'
    "There's a small round hole, freshly dug in the floor near the centre
    of the cave. "
;

+ boots : Wearable 'old wellington pair/boots/wellies' 'old pair of boots'
    "They look ancient, battered, and scuffed, but probably still waterproof. "
    initSpecialDesc = "A pair of old Wellington boots lies in the hole. "
;

caveNorthWall : DefaultWall 'north wall' 'north wall'
    "In the north wall is a narrow gap leading out of the cave. "
;

caveEastWall : DefaultWall 'east wall' 'east wall'
    "The east wall of the cave is quite smooth and has the faint remains of
    something drawn or written on it. Unfortunately it's no longer possible
    to discern whether it was once a Neolithic cave painting or an example
    of modern graffiti. "
;

/*
 *   ********************** THE COTTAGE GARDEN  *
 *************************/

cottageGarden : OutdoorRoom 'Cottage Garden'
    "This neat little garden is situated on the north side of the cottage. A
    stream runs along the bottom of the garden, while a short path disappears
    through a gap in the fence to the southeast, and another leads westwards down to
    the road. Next to the fence stands a small garden shed. "
    southeast = outsideCottage
    north : NoTravelMessage {"<<gardenStream.cannotCrossMsg>> "}
    east : NoTravelMessage {"You can't walk through the fence. "}
    west : FakeConnector {"That path leads down to the road, and you don't fancy
        going near all those nasty, smelly, noisy cars right now. " }
    in = insideShed
    vocabWords = '(cottage) garden'
;

+ Decoration 'cottage' 'cottage'
    "The neat little cottage is situated at the southern end of the garden,
    which it overlooks through a pair of small windows. ";
;

+ Decoration 'wooden fence' 'wooden fence'
    "The tall wooden fence runs along the eastern side of the garden, with
    a small gap at its southern end. "
;

+ gardenStream: Fixture 'stream' 'stream'
    "<<cannotCrossMsg>>"
    dobjFor(Cross)
    {
        verify() {}
        check() { failCheck(cannotCrossMsg); }
    }
    cannotCrossMsg = ' The stream is quite wide at this point, and too deep to cross. '
;

+ Openable, Enterable -> insideShed 'small wooden (garden) shed' 'garden shed'
    "It's a small, wooden shed. "
    dobjFor(LookIn) asDobjFor(Enter)

    /*
     *   The following commented out code shows how matchNameCommon() could
     *   be used here, but in practice it's easier in this case just to use
     *   'weak' tokens (i.e. tokens enclosed in parentheses) in our
     *   vocabWords property.
     */
    //  matchNameCommon(origTokens, adjustedTokens)
    //  {
    //    if(adjustedTokens.indexOf('shed'))
    //      return self;
    //    else
    //      return cottageGarden;
    //  }
;

/*
 *   ********************** INSIDE THE SHED    *
 *************************/

insideShed : Room 'Inside the Garden Shed'
    "The inside of the shed is full of garden implements, leaving just about enough
    room for one person to stand. An old cupboard stands in the corner. "
    out = cottageGarden
;

+ Decoration 'garden implements/hoe/rake/shears' 'garden implements'
    "There's a hoe, a rake, some shears, and several other bits and pieces. "
    isPlural = true
;

+ oars : Thing 'pair/oars' 'pair of oars'
    "The oars look like they're meant for a small rowing-boat. "
    bulk = 10
    initSpecialDesc = "A pair of oars leans against the wall. "
;

+ cupboard: ComplexContainer, Heavy 'battered old wooden cupboard' 'old cupboard'
    "The cupboard is a battered old wooden thing, with chipped blue and
    white paint. "
    subContainer : ComplexComponent, OpenableContainer { }
    subSurface : ComplexComponent, Surface { }
;

++ tin : OpenableContainer 'small square tin' 'small tin'
    "It's a small square tin with a lid. "
    subLocation = &subSurface
    bulkCapacity = 5
;

class Coin : Thing 'pound coin/pound*coins*pounds' 'pound coin'
    "It's gold in colour, has the Queen's head on one side and <q>One Pound</q>
    written on the reverse. The edge is inscribed with the words
    <q>DECUS ET TUTAMEN</q>"
    isEquivalent = true
;

+++  Coin;
+++  Coin;
+++  Coin;
+++  Coin;


++ torch : Flashlight, OpenableContainer 'small blue torch/flashlight' 'small blue torch'
    "It's just a small blue torch. "
    subLocation = &subContainer
    bulkCapacity = 1
    dobjFor(TurnOn)
    {
        check()
        {
            if(! battery.isIn(self))
            {
                "Nothing happens. ";
                exit;
            }
        }
    }
    iobjFor(PutIn)
    {
        check()
        {
            if(gDobj != battery)
            {
                "{The dobj/he} doesn't fit in the torch. ";
                exit;
            }
        }
        action()
        {
            inherited;
            makeOpen(nil);
            achieve.addToScoreOnce(1);
        }
    }
    notifyRemove(obj)
    {
        if(isOn)
        {
            "Removing the battery causes the torch to go out. ";
            makeOn(nil);
        }
    }
    achieve: Achievement
        { desc = "fitting the battery into the torch"  }
;

/*
 *   ********************** THE ROWING BOAT    *
 *************************/

boat : Heavy, Enterable -> insideBoat 'rowing boat/dinghy' 'rowing boat'
    //boat : Heavy 'rowing boat' 'rowing boat'
    @cottageGarden
    "It's a small rowing boat. "
    specialDesc = "A small rowing boat floats on the stream, just by the bank. "
    useSpecialDesc { return true; }
    dobjFor(Board) asDobjFor(Enter)
    dobjFor(Row)
    {
        verify()
        {
            illogicalNow('You need to be aboard the boat before you can row it. ');
        }
    }
    getFacets = [rowBoat]
;

boatBottom : Floor 'floor/bottom/(boat)' 'bottom of the boat'
;

insideBoat : OutdoorRoom
    name = ('In the boat (by '+ boat.location.name + ')')
    desc = "The boat is a plain wooden rowing dinghy with a single wooden seat. It
        is floating on the stream just by the <<boat.location.name>>. "
    out = (boat.location)
    roomParts = [boatBottom, defaultSky]
;

+ rowBoat: Fixture 'plain wooden rowing boat/dighy' 'boat'
    "<<insideBoat.desc>>"
    dobjFor(Take)
    {
        verify() {illogical('{You/he} can\'t take the boat - {you\'re/he\'s} in it!'); }
    }

    dobjFor(Row)
    {
        verify() {}
        check()
        {
            if(!oars.isHeldBy(gActor))
            {
                "{You/he} need to be holding the oars before you can row this boat. ";
                exit;
            }
        }
        action()
        {
            "You row the boat along the stream and under a low bridge, finally arriving
            at ";
            if(boat.isIn(jetty))
            {
                "the bottom of the cottage garden.<.p> ";
                boat.moveInto(cottageGarden);
            }
            else
            {
                "the side of a small wooden jetty.<.p> ";
                boat.moveInto(jetty);
            }
            nestedAction(Look);
        }
    }
    getFacets = [boat]
;

+ Chair, Fixture 'single wooden seat' 'wooden seat'
;

/*
 *   ********************** THE JETTY       *
 *************************/

jetty : OutdoorRoom 'Jetty'
    "This small wooden jetty stands on the bank of the stream. Upstream to the
    east you can see a road-bridge, and a path runs downstream along the bank
    to the west. Just to the south stands a small shop. "
    west : FakeConnector {"You could go wandering down the path but you don't
        feel you have much reason to. "}
    east : NoTravelMessage {"The path doesn't run under the bridge. "}
    south = insideShop
    is asExit(south)
;

+ Distant 'bridge' 'bridge'
    "It's a small brick-built hump-backed bridge carrying the road over
    the stream. "
;

+ Fixture 'stream' 'stream'
    "The stream becomes quite wide at this point, almost reaching the proportions
    of a small river. To the east it flows under a bridge, and to the west it
    carries on through the village. "
    dobjFor(Cross)
    {
        verify() {}
        check()
            { failCheck ('The stream is far too wide and deep to cross here. '); }
    }
;

+ Enterable -> insideShop 'small shop/store' 'shop'
    "The small, timber-clad shop has an open door, above which is a sign reading
    GENERAL STORE"
;

/*
 *   ********************** INSIDE THE SHOP   *
 *************************/


insideShop : Room 'Inside Shop' 'inside the shop'
    "The interior of the shop is lined with shelves containing all sorts of items,
    including basic foodstuffs, sweets, snacks, stationery, batteries, soft
    drinks and tissues. Behind the counter is a door marked 'PRIVATE'. "
    out = jetty
    north asExit(out)
    south : OneWayRoomConnector
    {
        destination = backRoom
        canTravelerPass(traveler) { return traveler != gPlayerChar; }
        explainTravelBarrier(traveler)
            { "The counter bars your way to the door. "; }
    }
;

+ Decoration 'private door*doors' 'door'
    "The door marked 'PRIVATE' is on the far side of the counter, and there seems to be
    no way you can reach it. The other door out to the jetty is to the north. "
;

+ Fixture, Surface 'counter' 'counter'
    "The counter is about four feet long and eighteen inches wide. "
;

++ bell : Thing 'small brass bell' 'small brass bell'
    "The bell comprises an inverted hemisphere with a small brass knob protruding
    through the top. Attached to the bell is a small sign. "
    dobjFor(Ring)
    {
        verify() {}
        check() {}
        action()
        {
            bellRing.triggerEvent(self);
        }
    }
;

+++ Component, Readable 'sign' 'sign'
    "The sign reads RING BELL FOR SERVICE. "
;

+++ Component 'knob/button' 'knob'
    "The knob protrudes through the top of the brass hemisphere of the bell. "
    dobjFor(Push) remapTo(Ring, bell)
;

+ Distant, Surface 'shelf*shelves' 'shelves'
    "The shelves with the most interesting goodies are behind the counter. "
    isPlural = true
;

++ batteries : Distant 'battery*batteries' 'batteries on shelf'
    "A variety of batteries sits on the shelf behind the counter. "
    isPlural = true
    salePrice = 3
    saleName = 'torch battery'
    saleItem = battery
;

++ sweets : Distant 'candy/sweets' 'sweets on shelf'
    "All sorts of tempting jars, bags, packets and boxes of sweets lurk temptingly
    on the shelves behind the counter. "
    isPlural = true
    salePrice = 1
    saleName = 'bag of sweets'
    saleItem = sweetBag
;

battery : Thing 'small red battery' 'small red battery'
    "It's a small red battery, 1.5v, manufactured by ElectroLeax
    and made in the People's Republic of Erewhon. "
    bulk = 1
;

sweetBag : Dispenser 'bag of candy/sweets' 'bag of sweets'
    "A bag of sweets. "
    canReturnItem = true
    myItemClass = Sweet
;

class Sweet : Dispensable, Food
    desc = "It's a small, round, clear, <<sweetGroupBaseName>> boiled sweet. "
    vocabWords = 'sweet/candy*sweets'
    location = sweetBag
    listWith = [sweetGroup]
    sweetGroupBaseName = ''
    collectiveGroup = sweetCollective
    sweetGroupName = ('one ' + sweetGroupBaseName)
    countedSweetGroupName(cnt)
        { return spellIntBelow(cnt, 100) + ' ' + sweetGroupBaseName; }
    tasteDesc = "It tastes sweet and tangy. "
    dobjFor(Eat)
    {
        action()
        {
            "You pop <<theName>> into your mouth and suck it. It tastes nice
            but it doesn't last as long as you'd like.<.p>";
            inherited;
        }
    }
;

class RedSweet : Sweet 'red - ' 'red sweet'
    isEquivalent = true
    sweetGroupBaseName = 'red'
;

class GreenSweet : Sweet 'green - ' 'green sweet'
    isEquivalent = true
    sweetGroupBaseName = 'green'
;

class YellowSweet : Sweet 'yellow - ' 'yellow sweet'
    isEquivalent = true
    sweetGroupBaseName = 'yellow'
;

sweetGroup: ListGroupParen
    showGroupCountName(lst)
    {
        "<<spellIntBelowExt(lst.length(), 100, 0,
                            DigitFormatGroupSep)>> sweets";
    }
    showGroupItem(lister, obj, options, pov, info)
        { say(obj.sweetGroupName); }
    showGroupItemCounted(lister, lst, options, pov, infoTab)
        { say(lst[1].countedSweetGroupName(lst.length())); }
;

sweetCollective: ItemizingCollectiveGroup 'candy*sweets' 'sweets'
;

RedSweet;
RedSweet;
RedSweet;
RedSweet;
GreenSweet;
GreenSweet;
GreenSweet;
YellowSweet;
YellowSweet;


bellRing : SoundEvent
    triggerEvent(source)
    {
        "TING!<.p>";
        inherited(source);
    }
;


backRoom: Room
    north = insideShop
;

SenseConnector, Intangible 'wall' 'wall'
    connectorMaterial = paper
    locationList = [backRoom, insideShop]
;

/*
 *   *********************************************************************
 *   NON PLAYER CHARACTERS                                              *
 ************************************************************************/

/*
 *   Definition of Joe Black, the Burner NPC - as this is complex we
 *   separate it from the other code and put it at the end
 */

burner : Person 'charcoal burner' 'charcoal burner'
    @fireClearing
    "It's rather difficult to make out his features under all the grime and soot. "
    properName = 'Joe Black'
    globalParamName = 'burner'
    isHim = true
;

+ GiveShowTopic @ring
    topicResponse
    {
        "As you hand the ring over to {the burner/him}, his eyes light up in delight
        and his jaws drop in amazement. <q>You found it!</q> he declares, <q>God bless
        you, you really found it! Now I can go and call on my sweetheart after all!
        Thank you, my dear, that's absolutely wonderful!</q><.p>";
        addToScore (2, 'giving {the burner/him} his ring back. ');
        /* CHANGED IN 3.0.6n */
        finishGameMsg(ftVictory, [finishOptionUndo,finishOptionFullScore]);

    }
;

+ DefaultGiveShowTopic, ShuffledEventList
    [
        '{The burner/he} shakes his head, <q>No thanks, love.</q>',
        'He looks at it and grins, <q>That\'s nice, my dear.</q> he remarks,
        handing it back.',
        '<q>I\'d hang on to that if I were you.</q> he advises. '
    ]
;

+ burnerTalking : InConversationState
    stateDesc = "He's standing talking with you. "
    specialDesc = "{The burner/he} is leaning on his spade
        talking with you. "
    nextState = burnerWorking
;

++ burnerWorking : ConversationReadyState
    stateDesc = "He's busily tending the fire. "
    specialDesc = "<<a++ ? '{The burner/he}' : '{A burner/he}'>>
        is walking round the fire, occasionally shovelling dirt onto it with his spade. "
    isInitState = true
    a = 0
;

+++ HelloTopic, StopEventList
    [
        '<q>Er, excuse me,</q> you say, trying to get {the\'s burner/her} attention.\b
        {The burner/he} moves away from the fire and leans on his spade to talk to
        you. <q>Hello, young lady. Mind you don\'t get too close to that fire now.</q>',
        '<q>Hello!</q> you call cheerfully.\b
        <q>Hello again!</q> {the burner/he} declares, pausing from his labours to
        rest on his spade.'
    ]
;

+++ ByeTopic
    "<q>Bye for now, then.</q> you say.<.p>
    <q>Take care, now.</q> {the burner/he} admonishes you as he returns to his work."
;

+++ ImpByeTopic
    "{The burner/he} gives a little shake of the head and returns to work."
;


++ AskTopic, SuggestedAskTopic @smoke
    "<q>Doesn't that smoke bother you?</q> you ask.<.p>
    <q>Nah! You get used to it - you just learn not to breathe too deep when it
    gets blown your way.</q> he assures you."
    name = 'the smoke'
;

++ AskTopic, SuggestedAskTopic, ShuffledEventList @fire
    [
        '<q>Why have you got that great big bonfire in the middle of the forest?</q>
        you ask.<.p>
        <q>It\'s not a bonfire, Miss, it\'s a fire for making charcoal.</q> he explains,
        <q>And to make charcoal I need to burn wood - slow like - and a forest is a
        good place to find wood - see?</q>',
        '<q>Doesn\'t it get a bit hot, working with that fire all day?</q> you wonder.<.p>
        <q>Yes, but it beats being cooped up in an office all day.</q> he replies,
        <q>I couldn\'t stand that!</q>',
        '<q>Why do you keep putting that dust on the fire?</q> you wonder.<.p>
        <q>To stop it burning too quick.</q> he tells you.'
    ]
    name = 'the fire'
;

++ AskTopic, SuggestedAskTopic @burner
    "<q>My name's Heidi.</q> you announce. <q>What's yours?</q><.p>
    <q><<burner.properName>>,</q> he replies, <q>Mind you, it'll soon be mud.</q>
    <.convnode burner-mud><<burner.makeProper>>"
    name = 'himself'
;

+++ AltTopic, SuggestedAskTopic, StopEventList
    [
        '<q>Have you been a charcoal burner long?</q> you ask.<.p>
        <q>About ten years.</q> he replies.',
        '<q>Do you like being a charcoal burner?</q> you wonder, <q>It seems
        rather messy!</q><.p>
        <q>It\'s better than being cooped up in some office or factory all day,
        at any rate.</q> he tells you.',
        '<q>What do you do when you\'re not burning charcoal?</q> you enquire.<.p>
        <q>Oh -- this and that.</q> he shrugs.'
    ]
    isActive = (burner.isProperName)
    name = 'himself'
;

++ AskTellTopic, SuggestedAskTopic, StopEventList @ring
    [
        '<q>What happened to the ring -- how did you manage to lose it?</q> you ask.<.p>
        <q>You wouldn\'t believe it.</q> he shakes his head again, <q>I took it out to
        take a look at it a couple of hours back, and then dropped the thing. Before
        I could pick it up again, blow me if a thieving little magpie didn\'t flutter
        down and fly off with it!</q>',
        '<q>Where do you think the ring could have gone?</q> you wonder.<.p>
        <q>I suppose it\'s fetched up in some nest somewhere,</q> he sighs, <q>Goodness
        knows how I\'ll ever get it back again!</q>',
        '<q>Would you like me to try to find your ring for you?</q> you volunteer
        earnestly.<.p>
        <q>Yes, sure, that would be wonderful.</q> he agrees, without sounding in the
        least convinced that you\'ll be able to.'
    ]
    name = 'the ring'
;

+++ AltTopic, SuggestedTellTopic
    "<q>I found a ring in a bird's nest, up a tree just down there.</q> you tell him,
    pointing vaguely southwards, <q>Could it be yours?</q><.p>
    <q>Really?</q> he asks, his eyes lighting up with disbelieving hope, <q>Let me
    see it!</q>"
    isActive = (gPlayerChar.hasSeen(ring))
    name = 'the ring'
;

++ AskForTopic @spade
    topicResponse
    {
        "<q>Could I borrow your spade, please?</q> you ask.<.p>
        <q>All right then,</q> he agrees a little reluctantly, handing you the spade,
        <q>but make sure you bring it back.</q>";
        spade.moveInto(gActor);
        getActor().setCurState(burnerFretting);
    }
;


++ DefaultAskTellTopic
    "<q>What do you think about <<gTopicText>>?</q> you ask.<.p>
    <q>Ah, yes indeed, <<gTopicText>>,</q> he nods sagely,
    <q><<rand('Quite so', 'You never know', 'Or there again, no indeed')>>.</q>"
;

+ ConvNode 'burner-mud';

++ SpecialTopic, StopEventList
    'deny that mud is a name'
    ['deny', 'that', 'mud', 'is', 'a', 'name']
    [
        '<q>Mud! What kind of name is that?<q> you ask.<.p>
        <q>My name -- tonight.</q> he replied gloomily.<.convstay>',
        '<q>But you can\'t <i>really</i> be called <q>Mud</q></q> you insist.<.p>
        <q>Oh yes I can!</q> he assures you.<.convstay>'
    ]
;

++ SpecialTopic 'ask why' ['ask','why']
    "<q>Why will your name be mud?</q> you want to know.<.p>
    He shakes his head, lets out an enormous sigh, and replies,
    <q>I was going to give her the ring tonight -- her engagement ring --
    only I've gone and lost it. Cost me two months' wages it did. And
    now she'll never speak to me again,</q> he concludes, with another
    mournful shake of the head, <q>never.</q><<gSetKnown(ring)>>"
;

++ DefaultAskTellTopic
    "<q>And why does...</q> you begin.<.p>
    <q>Mud.</q> he repeats with a despairing sigh.<.convstay>"
;

+ burnerFretting : InConversationState
    specialDesc = "{The burner/he} is standing talking to you with his
        hands on his hips. "
    stateDesc = "He's standing talking to you with his hands on his hips. "
    nextState = burnerWaiting
;

++ burnerWaiting : ConversationReadyState
    specialDesc = "{The burner/he} is walking round the fire, frowning as
        he keeps instinctively reaching for the spade that isn't there. "
    stateDesc = "He's walking round the fire. "
    afterTravel(traveler, connector)
    {
        getActor().initiateConversation(burnerFretting, 'burner-spade');
    }
;

+++ HelloTopic
    "<q>Hello, there.</q> you say.<.p>
    <q>Hello, young lady - have you brought my spade?</q> he asks."
;

+++ ByeTopic
    "<q>Bye, then.</q> you say.<.p>
    <q>Don't be too long with that spade -- be sure to bring it right back!</q>
    he admonishes you."
;

++ GiveShowTopic @spade
    topicResponse
    {
        "<q>Here's your spade then,</q> you say, handing it over.<.p>
        <q>Ah, thanks!</q> he replies taking it with evident relief. ";
        spade.moveInto(burner);
        burner.setCurState(burnerTalking);
    }

;

++ AskForTopic @spade
    "He doesn't have the spade. "
    isConversational = nil
;

++ AskTellTopic @spade
    "<q>This seems a very sturdy spade,</q> you remark.\b
    <q>It is -- look after it well, I need it for my work!</q> {the burner/he} replies."
;

+++ AltTopic
    "<q>I seem to have left your spade somewhere,</q> you confess.\b
    <q>I hope you can find it then!</q> {the burner/he} remarks anxiously."
    isActive = (!spade.isIn(burner.location))
;

++ DefaultAskTellTopic
    "<q>We can talk about that when I've got my spade back,</q> he tells you."
;

+ ConvNode 'burner-spade'
    npcGreetingMsg = "<.p>He looks up at your approach, and walks
        away from the fire to meet you. <q>Have you finished with my spade
        yet?</q> he enquires anxiously.<.p>"
    npcContinueMsg = "<q>What about my spade? Have you finished with it yet?</q>
        {the burner/he} repeats anxiously. "
;

++ YesTopic
    "<q>Yes, I have.</q> you reply.<.p>
    <q>Can I have it back then, please?</q> he asks."
;

++ NoTopic
    "<q>Not quite; can I borrow it a bit longer?</q> you ask.<.p>
    <q>Very well, then.</q> he conceded grudgingly, <q>But I need it
    to get on with my job, so please be quick about it.</q>"
;

/*
 *   Definition of Sally the Shopkeeper
 */


shopkeeper : SoundObserver, Person 'young shopkeeper/woman' 'young shopkeeper'
    @backRoom
    "The shopkeeper is a jolly woman with rosy cheeks and fluffy blonde curls. "
    isHer = true
    properName = 'Sally'
    notifySoundEvent(event, source, info)
    {
        if(event == bellRing && daemonID == nil && isIn(backRoom))
            daemonID = new SenseDaemon(self, &daemon, 2, self, sight);

        else if(isIn(insideShop) && event == bellRing)
            "<q>All right, all right, here I am!</q> says {the shopkeeper/she}.<.p>";

    }
    daemonID = nil
    daemon
    {
        moveIntoForTravel(insideShop);
        "{The shopkeeper/she} comes through the door and stands behind the counter.<.p>";
        daemonID.removeEvent();
        daemonID = nil;
        if(canTalkTo(gPlayerChar))
            initiateConversation(sallyTalking, 'sally-1');
    }
    globalParamName = 'shopkeeper'


    cashReceived = 0
    price = 0
    saleObject = nil
    // cashFuseID = nil
    // cashFuse
    // {
    //    if(saleObject == nil)
    //       {
    //         "<q>What's this for?</q> asks {the shopkeeper/she}, handing the money
    //         back, <q>Shouldn't you tell me what you want to buy first?</q>";
    //        cashReceived = 0;
    //       }
    //    else if(cashReceived < price)
    //      "<q>Er, that's not enough.</q> she points out, looking at you expectantly
    //        while she waits for the balance. ";
    //    else
    //    {
    //      "{The shopkeeper/she} takes the money and turns to take <<saleObject.aName>>
    //      off the shelf. She hands you <<saleObject.theName>> saying, <q>Here you are
    //      then";
    //      if(cashReceived > price)
    //        ", and here's your change";
    //      ".</q></p>";
    //      saleObject.moveInto(gPlayerChar);
    //      price = 0;
    //      cashReceived = 0;
    //      saleObject = nil;
    //    }
    //   cashFuseID = nil;
    // }
;


+ ConvNode 'sally-1'
    npcGreetingMsg = "<q>Hello, what can I get you?</q> she asks. <.p>"
;

+ sallyTalking : InConversationState
    specialDesc = "{The shopkeeper/she} is standing behind the counter talking
        with you. "
    stateDesc = "She's standing behind the counter talking with you. "
    nextState = sallyWaiting
;

++ sallyWaiting : ConversationReadyState
    specialDesc = "{The shopkeeper/she} is standing behind the counter, checking
        the stock on the shelves. "
    stateDesc = "She's checking the stock on the shelves behind the counter. "
    isInitState = true
    takeTurn
    {
        if(!gPlayerChar.isIn(insideShop) && shopkeeper.isIn(insideShop))
            shopkeeper.moveIntoForTravel(backRoom);
        inherited;
    }
;

+++ HelloTopic
    "<q>Hello, <<getActor.isProperName ? properName : 'Mrs Shopkeeper'>>,</q> you say.<.p>
    <q>Hello, <<getActor.isProperName ? 'Heidi' : 'young lady'>>, what can I do
    for you?</q> asks {the shopkeeper/she}."
;

+++ ByeTopic
    "<q>'Bye, then!</q> you say.<.p>
    <q>Goodbye<<isProperName ? ', Heidi' : nil>>. See you again soon!</q>
    {the shopkeeper/she} beams in return."
;

+++ ImpByeTopic
    "{The shopkeeper/she} turns away and starts checking the stock on the shelves."
;

++ AskTellTopic [shopkeeper, gPlayerChar]
    "<q>I'm Heidi. What's your name?</q> you ask.<.p>
    <q>Hello, Heidi; I'm <<shopkeeper.properName>>,</q> she smiles.
    <<shopkeeper.makeProper>>"
;

+++ AltTopic
    "<q>I'm feeling really <i>very</i> well today; how are you?</q> you ask.<.p>
    <q>I'm feeling very well too, thanks.</q> she tells you."
    isActive = (shopkeeper.isProperName)
;

++ AskTellTopic @burner
    "<q>Do you know {the burner/him}, the old fellow who works in the forest?</q>
    you enquire innocently.<.p>
    <q>He's not <i>that</i> old,</q> she replies coyly."
;

++ AskTellTopic @tWeather
    "<q>Lovely weather we're having, don't you think?</q> you remark.<.p>
    <q>Absolutely,</q> she agrees, <q>and with luck, it should stay fine tomorrow.</q>"
;

++ DefaultAskTellTopic, ShuffledEventList
    [
        '<q>What do you think about ' + gTopicText + '?</q> you ask.<.p>
            <q>Frankly, not a lot.</q> she replies.',
        '<q>I think it\'s really interesting that...</q> you begin.<.p>
        <q>Oh yes, really interesting.</q> she agrees.',
        'You make polite conversation about ' + gTopicText + ' and
            {the shopkeeper/she} makes polite conversation in return.'
    ]
;


++ DefaultGiveShowTopic
    "<q>No thanks, dear.</q> she says, handing it back. ";
;

++ GiveShowTopic
    matchTopic(fromActor, obj)
    {
        return obj.ofKind(Coin) ? matchScore : 0;
    }
    handleTopic(fromActor, obj)
    {
        //     if(shopkeeper.cashFuseID == nil)
        //       shopkeeper.cashFuseID = new Fuse(shopkeeper, &cashFuse, 0);
        shopkeeper.cashReceived ++;
        currency = obj;
        //     if(shopkeeper.cashReceived > 1)
        //         "number <<shopkeeper.cashReceived>>";
        if(shopkeeper.cashReceived <= shopkeeper.price)
            obj.moveInto(shopkeeper);
        /* add our special report */
        gTranscript.addReport(new GiveCoinReport(obj));

        /* register for collective handling at the end of the command */
        gAction.callAfterActionMain(self);

    }
    afterActionMain()
    {
        /*
         *   adjust the transcript by summarizing consecutive coin
         *   acceptance reports
         */
        gTranscript.summarizeAction(
            {x: x.ofKind(GiveCoinReport)},
            {vec:  'You hand over '
            + spellInt(vec.length())+' ' + currency.name+'s.\n'});
        if(shopkeeper.saleObject == nil)
        {
            "<q>What's this for?</q> asks {the shopkeeper/she}, handing the money
            back, <q>Shouldn't you tell me what you want to buy first?</q>";
            shopkeeper.cashReceived = 0;
        }
        else if(shopkeeper.cashReceived < shopkeeper.price)
            "<q>Er, that's not enough.</q> she points out, looking at you expectantly
            while she waits for the balance. ";
        else
        {
            "{The shopkeeper/she} takes the money and turns to take <<shopkeeper.saleObject.aName>>
            off the shelf. She hands you <<shopkeeper.saleObject.theName>> saying, <q>Here you are
            then";
            if(shopkeeper.cashReceived > shopkeeper.price)
                ", and here's your change";
            ".</q></p>";
            shopkeeper.saleObject.moveInto(gPlayerChar);
            shopkeeper.price = 0;
            shopkeeper.cashReceived = 0;
            shopkeeper.saleObject = nil;
        }

    }
    currency = nil
;

class GiveCoinReport: MainCommandReport
    construct(obj)
    {
        /* remember the coin we accepted */
        coinObj = obj;

        /* inherit the default handling */
        gMessageParams(obj);
        inherited('You hand over {a obj/him}. ');
    }

    /* my coin object */
    coinObj = nil
;



++ BuyTopic @batteries
    alreadyBought = "You only need one battery, and you've already bought it.<.p>"
;

++ BuyTopic @sweets
    alreadyBought = "You've already bought one bag of sweets. Think of your figure!
        Think of your teeth!<.p>"
;


class BuyTopic : AskAboutForTopic
    topicResponse
    {
        if(matchObj.saleItem.moved)
            alreadyBought;
        else if (shopkeeper.saleObject == matchObj.saleItem)
            "<q>Can I have the <<matchObj.saleName>>, please?</q> you ask.<.p>
            <q>I need another <<currencyString(shopkeeper.price -
                                               shopkeeper.cashReceived)>> from you.</q> she points out.<.p>";
        else if (shopkeeper.saleObject != nil)
            "<q>Oh, and I'd like a <<matchObj.saleName>> too, please.</q> you announce.<.p>
            <q>Shall we finish dealing with the <<shopkeeper.saleObject.name>> first?</q>
            {the shopkeeper/she} suggests. ";
        else
        {
            purchaseRequest();
            purchaseResponse();
            shopkeeper.price = matchObj.salePrice;
            shopkeeper.saleObject = matchObj.saleItem;
        }
    }
    alreadyBought = "You've already bought a <<matchObj.saleName>>.<.p>"
    purchaseRequest = "<q>I'd like a <<matchObj.saleName>> please,</q> you request.<.p>"
    purchaseResponse = "<q>Certainly, that'll be <<currencyString(matchObj.salePrice)>>,</q>
        {the shopkeeper/she} informs you.<.p>"
;


function currencyString(amount)
    {
        return spellInt(amount) + ' '  + ((amount>1) ? 'pounds' : 'pound');
    }

    tWeather : Topic 'weather';


/*
 *   ******************** DEBUGGING VERBS   *
 ***********************/

#ifdef __DEBUG

/*
 *   The purpose of the everything object is to contain a list of all usable
 *   game objects which can be used as a list of objects in scope for
 *   certain debugging verb. Everything caches a list of all relevant
 *   objects the first time its lst method is called.
 */

everything : object
    /*
     *   lst_ will contain the list of all objects. We initialize it to nil
     *   to show that the list is yet to be cached
     */
    lst_ = nil

    /*
     *   The lst_ method checks whether the list of objects has been cached
     *   yet. If so, it simply returns it; if not, it calls initLst to build
     *   it first (and then returns it).
     */

    lst()
    {
        if (lst_ == nil)
            initLst();
        return lst_;
    }

    /*
     *   initLst loops through every game object descended from Thing and
     *   adds it to lst_, thereby constructing a list of physical game
     *   objects.
     */
    initLst()
    {
        lst_ = new Vector(50);
        local obj = firstObj();
        while (obj != nil)
        {
            if(obj.ofKind(Thing))
                lst_.append(obj);
            obj = nextObj(obj);
        }
        lst_ = lst_.toList();
    }

;


DefineTAction(Purloin)
    cacheScopeList()
    {
        scope_ = everything.lst();
    }
;


VerbRule(Purloin)
    ('purloin'|'pn') dobjList
    :PurloinAction
    verbPhrase = 'purloin/purloining (what)'
;

modify Thing
    dobjFor(Purloin)
    {
        verify()
        {
            if(isHeldBy(gActor)) illogicalNow('{You/he} {is} already holding it. ');
        }
        check() {}
        action
        {
            mainReport('{The/he dobj} pops into your hands.\n ');
            moveInto(gActor);
        }
    }
;

modify Fixture
    dobjFor(Purloin)
    {
        verify {illogical ('That is not something you can purloin - it is fixed in place.'); }
    }
;

modify Immovable
    dobjFor(Purloin)
    {
        check()
        {
            "You can't take {the/him dobj}. ";
            exit;
        }
    }
;


DefineTAction(Gonear)

    //   cacheScopeList()
    //     {
    //       scope_ = everything.lst();
    //     }

    /*
     *   defining objInScope is an alternative to defining cacheScopeList in
     *   this particular situation; you can try out either way by commenting
     *   out one method and uncommenting in the other. This also applies to
     *   the Purloin verb.
     */

    objInScope(obj) { return true; }

;

VerbRule(Gonear)
    ('gonear'|'gn'|'go' 'near') singleDobj
    :GonearAction
    verbPhrase = 'gonear/going near (what)'
;

modify Thing
    dobjFor(Gonear)
    {
        verify() {}
        check() {}
        action()
        {
            local obj = self.roomLocation;
            if(obj != nil)
            {
                "{You/he} {are} miraculously transported...</p>";
                replaceAction(TravelVia, obj);
            }
            else
                "{You/he} can't go there. ";
        }
    }
;

modify Decoration
    dobjFor(Gonear)
    {
        verify() {}
        check() {}
        action() {inherited;}
    }

;

modify Distant
    dobjFor(Gonear)
    {
        verify() {}
        check() {}
        action() {inherited;}
    }
;


modify MultiLoc
    dobjFor(Gonear)
    {
        verify() { illogical('{You/he} cannot gonear {the dobj/him}, since it exists
            in more than one location. '); }
    }
    dobjFor(Purloin)
    {
        verify() { illogical('{You/he} cannot purloin {the dobj/him}, since it exists
            in more than one location. '); }
    }
;


#endif

